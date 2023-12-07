#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>  /* waitpid and macros */
#include <string.h>    /* memset */
#include <stdbool.h>
#include <time.h>      /* clock_gettime, clock_nanosleep */
#include <poll.h>

#include <tarp/common.h>
#include <tarp/error.h>
#include <tarp/log.h>
#include <tarp/timeutils.h>
#include <tarp/process.h>
#include <tarp/types.h>
#include <tarp/event.h>

#define ASYNC_PROCESS_REAPER_CHECK_INTERVAL_MS 100
#define NUM_STREAMS 3

/*
 * index in a 2-item array that stores the
 * results of a call to pipe(). */
enum pipeEnd{
    READ_END  = 0,
    WRITE_END = 1
};

/*
 * used to refer to the file descriptor number of the standard
 * streams of a process. */
enum stdStreamFdNo{
    STD_IN  = 0,
    STD_OUT = 1,
    STD_ERR = 2
};

// returns an integer corresponding to the exit status code of the
// process, or otherwise PROC_KILLED or PROC_NOSTATUS.
//
// Expects status to be set to PROC_NOSTATUS to start with if
// no status has been obtained from waitpid.
static int maybe_decode_process_exit_status(int status){
   // only try to dissect the status of the user-requested subprocess
   // if it has in fact been obtainen from waitpid.
    if (status == PROC_NOSTATUS)
        return status;

    if (WIFEXITED(status)){ /* 'normal' termination */
        status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)){
        /* terminated by signal -- either by the killer subprocess
         * or something else */
        status = PROC_KILLED;
    }

    return status;
}

/*
 * (1) child gets 0 and should break out of the switch and continue on.
 * (2) parent gets (positive) PID of child and should exit.
 */
int daemonize(void){
    ssize_t fd;

    switch(fork()){                     /* fork to background */
        case -1: return -1;
        case 0: break;                  /* (1) */
        default: exit(EXIT_SUCCESS);    /* (2) */
    }

    /* child carries on ...; note from this point on returning an error code
     * is useless as the original process will have already exited. Errors
     * must be reported via some other means (logging etc) */

    /* Become leader of a new session */
    THROWS_ON(setsid() == -1, ERROR_RUNTIMEERROR,
            "setsid failure: '%s'", strerror(errno));

    /* fork AGAIN to ensure the program can never reacquire
     * a controlling terminal, per SysV conventions */
    switch(fork()){
        case -1: THROWS(ERROR_RUNTIMEERROR, "Fork failure in child process");
        case 0 : break; /* child (grandchild of orig process) should continue */
        default: exit(EXIT_SUCCESS);
    }

    /* clear umask; do not disallow any permissions */
    umask(0);

    /* cd to /, so as not to prevent shutdown by keeping the fs from being
     * unmounted */
    WARN_ON((chdir("/") == -1), "FAILED to change directory to '/'");

    /* close stdin, stdout, stderr: use /dev/null instead so that IO
     * operations don't fail; /dev/null can always be written to,
     * discarding everything, and can always be read from, always giving EOF. */
    WARN_ON((close(STDIN_FILENO) == -1), "Failed to close stdin stream: '%s'",
            strerror(errno));

    /* point stdin to /dev/null; fd should be 0 (the stdin fd number) as Linux
     * reuses the smallest fd available -- which is STDIN_FILENO as it has been
     * closed above */
    fd = open("/dev/null", O_RDWR);
    WARN_ON((fd != STDIN_FILENO), "Failed to point stdin to /dev/null: '%s'",
            strerror(errno));

    /* point stderr and stdout to /dev/null as well. */
    WARN_ON( (dup2(STDIN_FILENO,  STDOUT_FILENO) != STDOUT_FILENO),
            "Failed to point stdout stream to /dev/null: '%s'", strerror(errno));
    WARN_ON( (dup2(STDIN_FILENO,  STDERR_FILENO) == STDERR_FILENO),
            "Failed to point stderr stream to /dev/null: '%s'", strerror(errno));

    return 0; /* only grand-child of original process reaches here */
}

/*
 * Check that the array of environment variables specified to
 * populate_environment is valid. That is, there is no pair
 * where one item is NULL and the other one is not.
 *
 * NOTE: envvars *must* have a {NULL, NULL} sentinel as the last entry.
 * If this is not the case, this function will read out of bounds.
 */
static bool is_valid_envspec(const struct string_pair envvars[]){
    assert(envvars);
    for (unsigned i=0;;++i){

        // both non NULL, valid
        if (envvars[i].first && envvars[i].second)
            continue;

        // both NULL, sentinel ==> done.
        if (!envvars[i].first && !envvars[i].second)
            return true;

        // one NULL, one non-NULL ==> invalid
        return false;
    }

    return true;
}

/*
 * Modify the environment of the current process.
 *
 * if vars is NULL, no values are set into the environment.
 * Otherwise it must be an array of string_pairs where the first
 * item is the key and the second item is the value of an environment
 * variable to set. The very last entry must be a {NULL, NULL} pair
 * to serve as a sentinel.
 *
 * The environment is populated with with the k-v pairs.
 * Existing matching keys are overwritten.
 *
 * If clear_first=true, the environment is emptied first before being
 * populated.
 *
 * <-- return
 * 0   on success.
 * 1   if clearenv() returned an error.
 * 2   if one or more of the entries were invalid (e.g. NULL key but non-NULL
 *     value or vice-versa).
 * -1  if setenv() returned an error for one or more entries. Errno can then
 *     be checked.
 */
static int populate_environment(
        const struct string_pair vars[],
        bool clear_first)
{
    if (clear_first){
        if (clearenv()) return 1;
    }

    if (!vars)                   return 0;
    if (!is_valid_envspec(vars)) return 2;

    int rc = 0;
    for (int i=0;; ++i){
        /* last element if both NULL */
        if (!vars[i].first && !vars[i].second) break;

        if (setenv(vars[i].first, vars[i].second, 1) == -1){
            error("setenv() error: '%s'", strerror(errno));
            return -1;
        }
    }

    return rc;
}

// close b and make it point to the same file description as a
#define DUPLICATE(a, b, fail_msg)                                         \
    do {                                                                  \
        if(dup2(a,b)==-1) error("%s: '%s'", fail_msg, strerror(errno));   \
    } while(0)

// point fd to /dev/null
static int attach_fd_to_dev_null(int fd){
    int nulldev_fd = open("/dev/null", O_RDWR);
    if (nulldev_fd == -1){
        error("Failed to open /dev/null: '%s'", strerror(errno));
        return -1;
    }

    DUPLICATE(nulldev_fd, fd, "Failed to duplicate /dev/null");
    close(nulldev_fd);
    return 0;
}

/*
 * Fork and exec a new process according to cmd.
 *
 * --> cmd
 * An array of strings specifying the process to execute. cmd *must not*
 * be NULL; the last string in cmd *must* be NULL.
 *
 * --> envvars
 * see populate_environment.
 *
 * --> clear_first
 * see populate_environment.
 *
 * --> instream, outstream, errstream
 * Make the subprocess use these for its stdin, stdout, and stderr respectively.
 * For each of these, the argument must be one of:
 *  - PROC_STREAM_ASIS or PROC_STREAM_DEVNULL
 *  - >0: otherwise it's expected the parameter is a valid file descriptor
 *        meant to either be read from or written to, as appropriate, and
 *        the respective stream will be attached to it.
 *
 * <-- return
 * -1 : fork failure; user can check errno as normal.
 * >0 : the pid of the subprocess forked.
 * NOTE: in case fork succeeds but exec fails, the forked subprocess will exit
 * with the errno set by exec. The exist status code of the subprocess
 * therefore in that case will be execv's errno value.
 */
static int fork_and_exec(
        const char *const cmd[],
        const struct string_pair envvars[],
        bool clear_first,
        int instream,
        int outstream,
        int errstream)
{
    int rc = 0;

    pid_t child_pid = fork();

    switch(child_pid){
    case -1:
        return -1;
    case 0:
        if (instream == STREAM_ACTION_DEVNULL){
            attach_fd_to_dev_null(STDIN_FILENO);
        } else if (instream != STREAM_ACTION_PASS){
            DUPLICATE(instream, STDIN_FILENO, "failed to dup stdin");
        }

        if (outstream == STREAM_ACTION_DEVNULL){
            attach_fd_to_dev_null(STDOUT_FILENO);
        } else if (outstream != STREAM_ACTION_PASS){
            DUPLICATE(outstream, STDOUT_FILENO, "failed to dup stdout");
        }

        if (errstream == STREAM_ACTION_DEVNULL){
            attach_fd_to_dev_null(STDERR_FILENO);
        } else if (errstream != STREAM_ACTION_PASS){
            DUPLICATE(errstream, STDERR_FILENO, "failed to dup stderr");
        }

        if ((rc = populate_environment(envvars, clear_first) != 0)){
            error("Error when populating environment");
        }

        //debug("calling execv with %s with %s", cmd[0], cmd[1]);
        execv(cmd[0], (char *const *)&cmd[1]);
        error("Failed to exec '%s': '%s'", cmd[0], strerror(errno));
        exit(errno);
    default:
        return child_pid;
    }
}

/*
 * Send the specified signal to the process with pid tgpid then optionally
 * reap it.
 *
 * NOTE: if you want to extract the exit status for the reaped process,
 * just pass reap=false and reap it after outside this function.
 */
static void try_kill(pid_t tgpid, int sig, bool reap){
    /* NOTE: if child has already exited but has not been wait()ed yet
     * (i.e. if it is a zombie), the signal is still 'delivered' in the
     * sense that kill will not return an error since it has been invoked
     * on an existing process. However, obviously you can't kill a dead
     * process and therefore the signal has no effect.
     * When wait() is eventually called, the status is the
     * initial one that the process initially exited with --- not the most
     * recent one as would be caused by this kill invocation.
     * This is in fact precisely what we want. */
    if (kill(tgpid, sig) == -1){
        error("Could not kill proc %d: '%s'", tgpid, strerror(errno));
        return;
    }

    // if killed, then optionally reap too
    if (reap){
        if (waitpid(tgpid, NULL, 0) != tgpid){
            error("waitpid() error: '%s'", strerror(errno));
        }
    }
}

/*
 * Fork a subprocess that sleeps for ms milliseconds then sends
 * a SIGKILL to the target pid on wakeup.
 *
 * This is a helper for being able to terminate a subprocess on a timeout.
 * NOTE: this is only necessary and only used for syncronous process
 * executions. For async, simpy register the killer as a timer event callback
 * instead (see async_exec below).
 *
 * <-- return
 * The pid of the killer process; the parent must reap the killer as well
 * as the process it killed.
 */
static inline pid_t fork_killer(pid_t target, uint32_t ms){
    int pid = fork();
    switch(pid){
    case -1: return -1;
    case  0:
        mssleep(ms, true);
        try_kill(target, SIGKILL, false); /* let parent process do the reaping */
        exit(0);
    default:
        return pid;
    }
}

// effectively an assert for sanity
static inline void validate_stdstream_action(enum stdStreamFdAction action){
    switch(action){
    case STREAM_ACTION_PASS:
    case STREAM_ACTION_DEVNULL:
    case STREAM_ACTION_PIPE:
    case STREAM_ACTION_JOIN:
        break;
    default:
        THROWS(ERROR_INVALIDVALUE, "invailid stdStreamFdAction constant (%d)", action);
    }
}

/*
 * Configure the settings for each standard process stream according to the
 * specification.
 *
 * Expects a pointer to a 3-item array of int_pairs (where the pairs correspond
 * to stdin [0], stdout [1], and stderr [2]) and a file descriptor or
 * an action flag (enum stdStreamFdAction) for each stream.
 * It will then populate the array accordingly.
 *
 * The int_pairs are populated such that .first is the descriptor to be
 * fed to the subprocess to bind it to the respective stream and .second,
 * if set, is the pipe end that the current process can read from
 * (stdout/stderr of subprocess) or write to (stdin of subprocess) it.
 *
 * <-- return
 * -1 on pipe() failure; 0 on sucess.
 */
static int set_process_std_stream_settings(
        struct int_pair descriptors[3],
        int stdin_action, /* either valid fd OR stdStreamFdAction */
        int stdout_action,
        int stderr_action
        )
{
    assert(descriptors);

    validate_stdstream_action(stdin_action);
    validate_stdstream_action(stdout_action);
    validate_stdstream_action(stderr_action);

    /* both stdout and stderr must specify STREAM_ACTION_JOIN if either of them
     * has specified it. */
    if (stdout_action == STREAM_ACTION_JOIN || stderr_action == STREAM_ACTION_JOIN)
        THROWS_ON(stdout_action != stderr_action, ERROR_INVALIDVALUE,
                "inconsistent stream action specification (STREAM_ACTION_JOIN "
                "for stdout and stderr");

    const size_t num_streams = 3;
    int pipefd[2];

    /* initialize. */
    for (unsigned i = 0; i < num_streams; ++i){
        descriptors[i].first = STREAM_ACTION_PASS;
        descriptors[i].second = STREAM_ACTION_PASS;
    }

    switch(stdin_action){
    case STREAM_ACTION_PASS:
        break;
    case STREAM_ACTION_DEVNULL:
        descriptors[STD_IN].first = STREAM_ACTION_DEVNULL;
        break;
    case STREAM_ACTION_JOIN: /* invalid for stdin; just assume ACTION_PIPE was meant */
    case STREAM_ACTION_PIPE:
        if (pipe(pipefd)) return -1;
        descriptors[STD_IN].first = pipefd[READ_END];
        descriptors[STD_IN].second = pipefd[WRITE_END];
        break;
    default:
        assert(false);
    }

    switch(stdout_action){
    case STREAM_ACTION_PASS:
        break;
    case STREAM_ACTION_DEVNULL:
        descriptors[STD_OUT].first = STREAM_ACTION_DEVNULL;
        break;
    case STREAM_ACTION_JOIN:
    case STREAM_ACTION_PIPE:
        if (pipe(pipefd)) return -1;
        descriptors[STD_OUT].first = pipefd[WRITE_END];
        descriptors[STD_OUT].second = pipefd[READ_END];
        break;
    default:
        assert(false);
    }

    switch(stderr_action){
    case STREAM_ACTION_PASS:
        break;
    case STREAM_ACTION_DEVNULL:
        descriptors[STD_ERR].first = STREAM_ACTION_DEVNULL;
        break;
    case STREAM_ACTION_JOIN: /* use same as stdout */
        descriptors[STD_ERR].first = pipefd[WRITE_END];
        descriptors[STD_ERR].second = pipefd[READ_END];
        break;
    case STREAM_ACTION_PIPE:
        if (pipe(pipefd)) return -1;
        descriptors[STD_ERR].first = pipefd[WRITE_END];
        descriptors[STD_ERR].second = pipefd[READ_END];
        break;
    default:
        assert(false);
    }

    return 0;
}

// helper to ensure the standard stream fds are closed if they
// were opened by this library (which is the case if pipes were implicitly
// created).
//
// NOTE: if a pipe was created for a given stream, then both .first and ,second
// (see below) are file descriptors (>=0).
//
static inline void close_fds_if_required(struct int_pair fds[3]){
    assert(fds);

    int first, second;
    first = fds[STD_IN].first; second = fds[STD_IN].second;
    if (first != STREAM_ACTION_PASS && second != STREAM_ACTION_PASS){
        close(first); close(second);
    }

    first = fds[STD_OUT].first; second = fds[STD_OUT].second;
    if (first != STREAM_ACTION_PASS && second != STREAM_ACTION_PASS){
        close(first); close(second);
    }

    // if STREAM_ACTION_JOIN was used, then same as stdout, so already closed
    if (fds[STD_ERR].first == first && fds[STD_ERR].second == second) return;

    first = fds[STD_ERR].first; second = fds[STD_ERR].second;
    if (first != STREAM_ACTION_PASS && second != STREAM_ACTION_PASS){
        close(first); close(second);
    }
}

#define POLL_MS_TIMEOUT          1
/* see tarp/process.h for an explanation on the error codes. */
int sync_exec(
        const char *const cmdspec[],
        const struct string_pair envvars[],
        bool clear_first,
        const int instream,
        int outstream,
        int errstream,
        int ms_timeout,
        int *status,
        pid_t *pid_,
        process_ioevent_cb ioevent_cb,
        void *priv
        )
{
    assert(cmdspec);
    pid_t pid, proc_pid, killer_pid;
    int exit_status = PROC_NOSTATUS;
    bool use_killer = (ms_timeout > -1);

    struct int_pair desc[NUM_STREAMS];
    if (set_process_std_stream_settings(desc, instream, outstream, errstream)){
        close_fds_if_required(desc);
        return 2;
    }

    proc_pid = fork_and_exec(cmdspec, envvars, clear_first,
            desc[STD_IN].first, desc[STD_OUT].first, desc[STD_ERR].first);
    if (pid_) *pid_ = proc_pid;  /* out param */
    if (proc_pid == -1){
        close_fds_if_required(desc);
        return -1;
    }

    if (use_killer){
        if ((killer_pid = fork_killer(proc_pid, ms_timeout)) == -1){
            error("Failed to fork killer; killing process now.");
            try_kill(proc_pid, SIGKILL, true);
            close_fds_if_required(desc);
            return 1;
        }
    }

    /* NOTE: if second is -1, then the respective entry is simply ignored by
     * poll */
    struct pollfd fds[NUM_STREAMS] = {
        {.fd = desc[STD_IN].second, .events = POLLOUT, .revents = 0},
        {.fd = desc[STD_OUT].second, .events = POLLIN, .revents = 0},
        {.fd = desc[STD_ERR].second, .events = POLLIN, .revents = 0},
    };

    /* set to true when reaped so as not to attempt to do it
     * outside the loop again */
    bool proc_reaped = false;
    bool killer_reaped = false;

    /* stop loop because of waitpid error; NOTE: call waitpid for
     * both children first and only break after.
     * (1), (2): if waitpid returns a value != 0, then must stop either
     * because there has been an error or because the expected
     * process has been reaped. */
    bool must_stop   = false;

    for (;;){
        if (use_killer){
            if ((pid = waitpid(killer_pid, NULL, WNOHANG))){
                must_stop = true; /* (1) */
                if (pid == killer_pid) killer_reaped = true;
            }
        }

        if ((pid = waitpid(proc_pid, &exit_status, WNOHANG))){
            must_stop = true; /* (2) */
            if (pid == proc_pid) proc_reaped = true;
        }

        if (must_stop) break;

        int events = poll(fds, ARRLEN(fds), POLL_MS_TIMEOUT);
        if (!events) continue;

        for (size_t i = 0; i < NUM_STREAMS; ++i){
            if (ioevent_cb && fds[i].revents > 0){
                /* if STREAM_ACTION_JOIN was used, only call callback
                 * once for stdout and stderr combined, else the 2nd
                 * invocation would block */
                if (i == STD_ERR && fds[STD_ERR].fd == fds[STD_OUT].fd) continue;
                ioevent_cb(i, fds[i].fd, fds[i].revents, priv);
            }
        }
    }

    // reap any process that has not been reaped
    if (use_killer && !killer_reaped){
        try_kill(killer_pid, SIGKILL, true);
    }

    if (!proc_reaped){
        try_kill(proc_pid, SIGKILL, false);
        waitpid(proc_pid, &exit_status, WNOHANG);
    }

    exit_status = maybe_decode_process_exit_status(exit_status);
    if (status) *status = exit_status;
    close_fds_if_required(desc);
    return 0;
}

/*
 * All state of an asynchronous process -- mainly relating to callbacks
 * registered with the event pump, descriptors opened for the standard
 * process streams (if pipes have been implicitly created) etc.
 * The state is allocated dynamically and is freed when the process is
 * eventually reaped. */
struct async_process_state {
    struct timer_event killer;
    struct timer_event reaper;
    struct fd_event std_in;
    struct fd_event std_out;
    struct fd_event std_err;
    struct int_pair fds[3];
    pid_t pid;
    struct evp_handle *evp;
    process_ioevent_cb    ioevent_cb;
    process_completion_cb completion_cb;
    void *user_priv;
};

// cyclic timer: periodicaly tries to reap process and ultimately stops
// once process is reaped.
static void async_process_reaper(struct timer_event *tev, void *priv){
    assert(tev);
    assert(priv);

    int pid;
    int exit_status = PROC_NOSTATUS;

    struct async_process_state *state = priv;

    /* if not reaped, try again later */
    if (!(pid = waitpid(state->pid, &exit_status, WNOHANG))){
        Evp_set_timer_interval_ms(&state->reaper,
                ASYNC_PROCESS_REAPER_CHECK_INTERVAL_MS);
        Evp_register_timer(state->evp, &state->reaper);
        return;
    }

    exit_status = maybe_decode_process_exit_status(exit_status);
    void *user_priv = state->user_priv;
    process_completion_cb completion_cb = state->completion_cb;

    /* process done. Destroy all state. */
    if (state->fds[STD_IN].second != -1)
        Evp_unregister_fdmon(state->evp, &state->std_in);

    if (state->fds[STD_OUT].second != -1)
        Evp_unregister_fdmon(state->evp, &state->std_out);

    // NOTE: this will have been set to -1 by async_exec if STREAM_ACTION_JOIN
    // was used for both stdout to stderr. This avoids a double-close in
    // close_fds_if_required and such
    if (state->fds[STD_ERR].second != -1)
        Evp_unregister_fdmon(state->evp, &state->std_err);

    /*
     * Unregister reaper and killer just in case they are still
     * scheduled;
     * NOTE: this situation arises when the process is killed
     * on request: the killer is called immediately, directly,
     * and then so is the reaper.
     * This is even though there may be scheduled timer callbacks
     * for either or both. We free the state here so any scheduled
     * callbacks would cause a use-after free. Therefore they must
     * be unscheduled. */
    Evp_unregister_timer(state->evp, &state->killer);
    Evp_unregister_timer(state->evp, &state->reaper);

    close_fds_if_required(state->fds);
    free(state);

    if (completion_cb)
        completion_cb(pid, exit_status, user_priv);
}

/*
 * Gets called for any event on implicitly-created pipes for the process's
 * standard streams.
 * Dispatches to the user-supplied ioevent_cb. Note if the user does not provide
 * an ioevent_cb, then this wrapper does not get called.
 */
static void async_process_fd_event_cb_wrapper(struct fd_event *fdev, int fd, void *priv){
    assert(priv);
    assert(fdev);

    struct async_process_state *state = priv;
    assert(state->ioevent_cb);
    if (fd == state->fds[STD_IN].second){
        state->ioevent_cb(STDIN, fd, FD_EVENT_WRITABLE, state->user_priv);
    } else if (fd == state->fds[STD_OUT].second){
        state->ioevent_cb(STDOUT, fd, FD_EVENT_READABLE, state->user_priv);
    } else if (fd == state->fds[STD_ERR].second){
        state->ioevent_cb(STDERR, fd, FD_EVENT_READABLE, state->user_priv);
    }
}

// one-shot timer callback to kill a process on specified timeout.
void async_process_killer(struct timer_event *tev, void *priv){
    assert(priv);

    /* NOTE: this may potentially be null as in some circumstances
     * this function will be called directly rather than by the
     * event pump API. This is for example when a process is to be killed
     * on request rather than wait for a/the scheduler kiler. NOTE this
     * function is a one-shot timer callback and tev is not used here anyway.
     * */
    UNUSED(tev);

    struct async_process_state *state = (struct async_process_state *)priv;

    try_kill(state->pid, SIGKILL, false);

    /* reap *now* */
    async_process_reaper(&state->reaper, state);
}

/*
 * NOTE:
 * if process_state is not NULL, the pointer to the internal process
 * state associated with the asynchronous process is set in it.
 * This is useful internally if the process must be terminated prematurely
 * e.g. on user request. See the .stop() method in process.cxx.
 */
int async_exec__(
        struct evp_handle *event_pump,
        const char *const cmdspec[],
        const struct string_pair envvars[],
        bool clear_first,
        int instream,
        int outstream,
        int errstream,
        int ms_timeout,
        pid_t *pid,
        void **process_state,
        process_ioevent_cb ioevent_cb,
        process_completion_cb completion_cb,
        void *priv
        )
{
    assert(event_pump);
    assert(cmdspec);

    int rc;
    bool use_killer = (ms_timeout > -1);

    struct async_process_state *state =
        salloc(sizeof(struct async_process_state), NULL);

    state->completion_cb = completion_cb;
    state->ioevent_cb = ioevent_cb;
    state->evp = event_pump;
    state->user_priv = priv;
    if (process_state) *process_state = state;

    if (set_process_std_stream_settings(state->fds, instream, outstream, errstream))
        goto cleanup;

    rc = fork_and_exec(cmdspec, envvars, clear_first, state->fds[STD_IN].first,
            state->fds[STD_OUT].first, state->fds[STD_ERR].first);
    if (pid) *pid = rc;
    if (rc == -1) goto cleanup;
    state->pid = rc;

    if (state->fds[STD_IN].second != STREAM_ACTION_PASS){
        Evp_init_fdmon(&state->std_in, state->fds[STD_IN].second,
                FD_EVENT_WRITABLE, async_process_fd_event_cb_wrapper, state);
        Evp_register_fdmon(state->evp, &state->std_in);
    }

    if (state->fds[STD_OUT].second != STREAM_ACTION_PASS){
        Evp_init_fdmon(&state->std_out, state->fds[STD_OUT].second,
                FD_EVENT_READABLE, async_process_fd_event_cb_wrapper, state);
        Evp_register_fdmon(state->evp, &state->std_out);
    }

    if (state->fds[STD_ERR].second != STREAM_ACTION_PASS){
        if (state->fds[STD_ERR].second != state->fds[STD_OUT].second){
            Evp_init_fdmon(&state->std_err, state->fds[STD_ERR].second,
                    FD_EVENT_READABLE, async_process_fd_event_cb_wrapper, state);
            Evp_register_fdmon(state->evp, &state->std_err);
        } else {  /* to simplify reaper cleanup logic */
            state->fds[STD_ERR] = (struct int_pair){-1,-1};
        }
    }

    Evp_init_timer_ms(&state->reaper, ASYNC_PROCESS_REAPER_CHECK_INTERVAL_MS,
            async_process_reaper, state);
    Evp_register_timer(state->evp, &state->reaper);

    if (use_killer){
        Evp_init_timer_ms(&state->killer, ms_timeout, async_process_killer, state);
        Evp_register_timer(state->evp, &state->killer);
    }

    return 0;

cleanup:
    close_fds_if_required(state->fds);
    salloc(0, state);
    return -1;
}

int async_exec(
        struct evp_handle *event_pump,
        const char *const cmdspec[],
        const struct string_pair envvars[],
        bool clear_first,
        int instream,
        int outstream,
        int errstream,
        int ms_timeout,
        pid_t *pid,
        process_ioevent_cb ioevent_cb,
        process_completion_cb completion_cb,
        void *priv
        )
{
    return async_exec__(event_pump, cmdspec, envvars, clear_first,
            instream, outstream, errstream, ms_timeout,
            pid, NULL, ioevent_cb, completion_cb, priv);
}

