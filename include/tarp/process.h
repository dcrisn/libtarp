#ifndef TARP_PROCESS_H
#define TARP_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif


#include <sys/types.h>

#include <tarp/types.h>
#include <tarp/event.h>

struct evp_handle;

enum stdStream {
    STDIN = 0,
    STDOUT = 1,
    STDERR = 2
};

/*
 * Flags that can be passed to sync_exec and async_exec (instead of a file
 * descriptor) when specifying the standard input, output and error
 * configuration for the subprocess.
 *
 * (1) Like python 'pass'. I.e. do nothing, NOP. Leave stream as is.
 *
 * (2) Effectively close the stream but with well-defined semantics.
 * (I.e. NOTE actually calling close() on the descriptors beloning to stdin,
 * stdout is not a good idea.)
 *
 * (3) implicitly create a pipe for this stream, poll the descriptor (read-end
 * for stdin, write-end for stdout/stderr), and invoke the user callback on
 * events (write-end of stdin pipe is writable, read-end of stdout and stderr
 * pipe(s) is readable). The callback can
 * demultiplex between the streams by the stdStream enumeration value.
 *
 * (4) Create a pipe like (3). This may only be specified for both stdout
 * AND stderr together at the same time (IOW it is a fatal error to specify
 * this for one but not the other). This makes it so the same pipe is used
 * for both stdout and stderr -- IOW the child process sends its stderr and
 * stdout to (and this process reads them from) the same stream.
 */
enum stdStreamFdAction {
    STREAM_ACTION_PASS = -1,      /* (1) */
    STREAM_ACTION_DEVNULL = -2,   /* (2) */
    STREAM_ACTION_PIPE = -3,      /* (3) */
    STREAM_ACTION_JOIN = -4,      /* (4) */
};

/*
 * The exit status of a process as returned to the caller is one of the
 * following:
 *
 * - PROC_NOSTATUS
 *   an exit status code could not be obtained. Something weird
 *   went on with the execution of this process (possibly an internal bug
 *   in this library)
 *
 * - PROC_KILLED
 *   process was killed. This is either because a timeout was
 *   specified by the user and the timeout killer killed the process. Or
 *   something else killed it (e.g. perhaps the process received a SIGSEGV
 *   from the kernel, as an example).
 *
 *  - >=0
 *  The process exited normally in the sense that the library got back an
 *  exit status code from it. This value is whatever the process passed to
 *  exit()/exit_() -- or, if exec() failed (i.e. the subprocess forked but
 *  the user-specified command could not be executed), it is set to the errno
 *  value set by execv.
 */
#define PROC_NOSTATUS  -1
#define PROC_KILLED    -2

typedef void (*process_ioevent_cb)(
        enum stdStream stream, int fd, unsigned events, void *priv);

typedef void (*process_completion_cb)(pid_t pid, int status, void *priv);

/*
 * Turn the calling process into a daemon.
 *
 * <-- return
 *     -1 on failure (failed to fork current process) and errno is set
 *     as appropriate. See man 2 fork fmi.
 *     Otherwise if successful, the current process exits, and only
 *     the forked child (actually, ultimately the grandchild) process
 *     continues.
 */
int daemonize(void);

/*
 * Run a subprocess synchronously.
 *
 * This function creates a subprocess according to cmdspec, sets its
 * environment according to envvars, configures its standard streams according
 * to instream, outstream, and errstream, and lets it run for ms_timeout
 * milliseconds before terminating it.
 *
 * See fork_and_exec fmi on cmdspec, envvars, clear_first, instream,
 * outstream, and errstream.
 *
 * if ms_timeout == -1, then the subprocess is never terminated by this
 * function. That is, no 'killer' subprocess is spawned (see fork_and_exec
 * fmi) and this function only returns when the subprocess exits by itself
 * (or an error occurs when creating it or wait()-ing on it).
 *
 * Otherwise if ms_timeout > -1, then this function will attempt to terminate
 * the subprocess forcibly if it's run for more than timeout_ms milliseconds.
 *
 * --> cb, @optional
 *  callback to invoke whenever there is an event on instream (POLLOUT),
 *  outstream (POLLIN), errstream (POLLIN) or extra_fd (POLLIN|POLLOUT).
 *  NOTE: this calback is only called if STREAM_ACTION_PIPE
 *  (or STREAM_ACTION_JOIN) was passed for any of instream, outstream or errstream
 *  as appropriate.
 *
 * --> priv
 * Pointer to anything the user wants passed to the callback when it gets
 * invoked.
 *
 * --> status, @optional
 *  if not NULL, the exit status of the subprocess will be stored in this
 *  variable. NOTE: if this function returns an error code that indicates
 *  the subprocess was never forked, then status must be ignored.
 *  Otherwise it is one of PROC_NOSTATUS, PROC_KILLED, or a value >= 0.
 *  See top of file for an explanation on the semantics of these values.
 *
 * <-- return
 * 0 : success - the subprocess was forked, executed, and reaped. User should
 *     check 'status' to detemine how exactly the process terminated (see
 *     details above).
 *
 * -1: failed to fork subprocess; user can check errno as set by fork.
 *
 * 1 : ms_timeout was specified as > -1, but the killer subprocess could not
 *     be forked. The user-requested subprocess in this case is killed
 *     immediately. status must be ignored. The function call is considered
 *     to have failed.
 *
 * 2 : failed to create pipes (errno can be checked for the error set by pipe()).
 */
int sync_exec(
        const char *const cmdspec[],
        struct string_pair envvars[],
        bool clear_first,
        int instream,
        int outstream,
        int errstream,
        int ms_timeout,
        int *status,
        void (*ioevent_cb)(enum stdStream stream, int fd, unsigned events, void *priv),
        void *priv);

/*
 * Execute a process and wait for its completion asynchronoulsy.
 *
 * This is much like sync_exec (see that for details on the parameters)
 * with the difference that this is integrated to be used with the
 * event pump api (see tarp/event.h).
 * The call does not block and synchronously wait for a process to complete
 * the way sync_exec does. Instead, if the user is interested in the exit
 * status, they must provide a process_completion_cb that is called right after
 * the process has been reaped. NOTE: the pid passed to the callback is
 * informative only and *was* the pid of the now-exited process.
 * The user's process does not exist anymore by this stage and is therefore no
 * longer associated with pid.
 *
 * Both ioevent_cb and completion_cb are optional.
 *
 * priv is passed as-is to *both* ioevent_cb and completion_cb.
 */
int async_exec(
        struct evp_handle *event_pump,
        const char *const cmdspec[],
        struct string_pair envvars[],
        bool clear_first,
        int instream,
        int outstream,
        int errstream,
        int ms_timeout,
        process_ioevent_cb ioevent_cb,
        process_completion_cb completion_cb,
        void *priv);


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif
