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
#include <tarp/timeutils.h>

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
static bool is_valid_envspec(struct string_pair envvars[]){
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
        struct string_pair vars[],
        bool clear_first)
{
    if (clear_first){
        if (clearenv()) return 1;
    }

    if (!vars)                return 0;
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
int attach_fd_to_dev_null(int fd){
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
 * For each of these, if the argument is:
 *  - -1: nothing is done. It's a NOP.
 *  - -2: the respective stream is pointer to /dev/null i.e. the respective
 *        stream is effectively closed.
 *  - >0: otherwise it's expected the parameter is a valid file descriptor
 *        meant to either be read from or written to, as appropriate, and
 *        the respective stream will be attached to it.
 *
 * <-- return
 * -1 : fork failure; user can check errno as normal.
 * >0 : the pid of the subprocess forked.
 * NOTE: in case fork succeeds but exec fails, the forked subprocess will exit
 * with the errno set by exec.
 */
static int fork_and_exec(
        const char *const cmd[],
        struct string_pair envvars[],
        bool clear_first,
        int instream,
        int outstream,
        int errstream
        )
{
    int rc = 0;

    pid_t child_pid = fork();
    switch(child_pid){
    case -1:
        return -1;
    case 0:
        if (instream == -2){
            attach_fd_to_dev_null(STDIN_FILENO);
        } else if (instream != -1){
            DUPLICATE(instream, STDIN_FILENO, "failed to dup stdin");
        }

        if (outstream == -2){
            attach_fd_to_dev_null(STDOUT_FILENO);
        } else if (outstream != -1){
            DUPLICATE(outstream, STDOUT_FILENO, "failed to dup stdout");
        }

        if (errstream == -2){
            attach_fd_to_dev_null(STDERR_FILENO);
        } else if (errstream != -1){
            DUPLICATE(errstream, STDERR_FILENO, "failed to dup stderr");
        }

        if ((rc = populate_environment(envvars, clear_first) != 0)){
            error("Error when populating environment");
        }

        debug("calling execv with %s with %s", cmd[0], cmd[1]);
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
     * process. And when wait() is eventually called, the status is the
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
 * Fork a subprocess that sleps for ms milliseconds then sends
 * a SIGKILL to the target pid on wakeup.
 *
 * This is a helper for being able to terminate a subprocess on a timeout.
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

#define POLL_MS_TIMEOUT          1
#define EXIT_STATUS_PLACEHOLDER -1
#define RC_SIGNAL_TERMINATION   -2
int sync_exec(
        const char *const cmdspec[],
        struct string_pair envvars[],
        bool clear_first,
        int instream,
        int outstream,
        int errstream,
        int extra_fd,  /* used for e.g. read-end of pipe */
        int ms_timeout,
        int *status,
        void (*cb)(int fd, unsigned events, void *priv),
        void *priv
        )
{

    pid_t pid, proc_pid, killer_pid;
    int exit_status = EXIT_STATUS_PLACEHOLDER;
    bool use_killer = (ms_timeout > -1);

    proc_pid = fork_and_exec(cmdspec, envvars, clear_first, instream, outstream, errstream);
    if (proc_pid == -1) return -1;

    if (use_killer){
        if ((killer_pid = fork_killer(proc_pid, ms_timeout)) == -1){
            error("Failed to fork killer; killing process now.");
            try_kill(proc_pid, SIGKILL, true);
            return 1;
        }
    }

    struct pollfd fds[4] = {
        {
          .fd       = instream,
          .events   = POLLOUT,
          .revents  = 0
        },
        {
          .fd      = outstream,
          .events  = POLLIN,
          .revents = 0
        },
        {
          .fd      = errstream,
          .events  = POLLIN,
          .revents = 0
        },
        {
           .fd      = extra_fd,
           .events  = POLLIN | POLLOUT,
           .revents = 0
        }
    };

    /* set to true when reaped so as not to attempt to do it
     * outside the loop again */
    bool proc_reaped = false;
    bool killer_reaped = false;

    /* stop loop because of waitpid error; NOTE: call waitpid for
     * both children first and only break after.
     * (1), (2): if waitpid returns a value != 0, then stop either
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

        for (int i = 0; i < (ssize_t)ARRLEN(fds); ++i){
            if (cb && fds[i].revents > 0) cb(fds[i].fd, fds[i].revents, priv);
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

    // only try to dissect the status of the user-requested subprocess
    // if it could be obtained.
    if (exit_status != EXIT_STATUS_PLACEHOLDER){
        if (WIFEXITED(exit_status)){ /* 'normal' termination */
            exit_status = WEXITSTATUS(exit_status);
        } else if (WIFSIGNALED(exit_status)){
            /* terminated by signal -- either by the killer subprocess
             * or something else */
            exit_status = RC_SIGNAL_TERMINATION;
        }
    }

    if (status) *status = exit_status;
    return 0;
}

