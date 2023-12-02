#ifndef TARP_PROCESS_H
#define TARP_PROCESS_H

#include <tarp/types.h>

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
 * environment according to envvars, sets its standard streams according
 * to instream, outstream, and errstream, and lets it run for ms_timeout
 * milliseconds before terminating it.
 *
 * See fork_and_exec fmi on cmdspec, envvars, clear_first, instream,
 * outstream, and errstream.
 *
 * if ms_timeout == -1, then the subprocess is never terminated by this
 * function. That is, no 'killer subprocess' is spawned (see fork_and_exec
 * fmi) and this function only returns when the subprocess exits by itself
 * (or an error occurs when creating it or wait()-ing on it).
 *
 * Otherwise if ms_timeout > -1, then this function will attempt to terminate
 * the subprocess forcibly if it's run for more than timeout_ms milliseconds.
 * If instream, errstream, and/or outstream are valid descriptors > -1, then
 * all of them are polled and if cb is non-NULL, it is invoked for any of them
 * any time there is a POLLIN event (for outstream and errstream) or POLLOUT
 * event (instream). priv is an arbitrary pointer specified by the user to
 * be passed to cb whenever it is invoked.
 * NOTE: instream, errstream, outstream do not necessarily need to be the same
 * value.
 *
 * --> extra_fd
 *  This is an extra file descriptor that can be specified so that this
 *  function adds it to poll() and calls the cb for it (in addition to
 *  instream, errstream, outstream, depending on how they are specified).
 *  This is particularly useful in a scenario such as the following: the
 *  write end of a pipe is passed as outstream and/or errstream and the
 *  read-end is passed as extra_fd and a pointer to it is also passed as
 *  the priv pointer. This way when the callback gets invoked, it can
 *  easily determine that it has been invoked for the descriptor that
 *  represents the read-end of the pipe and then read from the pipe as
 *  appropriate.
 *
 * --> cb, @optional
 *  callback to invoke whenever there is an event on instream (POLLOUT),
 *  outstream (POLLIN), errstream (POLLIN) or extra_fd (POLLIN|POLLOUT).
 *
 * --> priv
 * Pointer to anything the user wants passed to the callback when it gets
 * invoked.
 *
 * --> status, @optional
 *  if not NULL, the exit status of the subprocess will be stored in this
 *  variable. NOTE: if this function returns an error code that indicates
 *  the subprocess was never forked, then status must be ignored.
 *  Otherwise:
 *   - if status == -1, the exit statsus of the process could not be determined.
 *     Something went fundamentally wrong and the process couldn't be wait()ed
 *     on.
 *   - if status == -2, then the process was terminated by a signal (which
 *     may have been the killer subprocess -- i.e. due to the user-requested
 *     process having timed out -- or may have been something else).
 *   - if status >= 0, then the subprocess terminated normally. 'status' in this
 *     case contains the error code the process exited with.
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
 */
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
        void *priv);

#endif
