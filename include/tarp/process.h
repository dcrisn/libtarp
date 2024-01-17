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
 * --> ioevent_cb, @optional
 *  callback to invoke whenever there is an event on instream (POLLOUT),
 *  outstream (POLLIN), errstream (POLLIN).
 *  NOTE: this calback is only called if STREAM_ACTION_PIPE
 *  (or STREAM_ACTION_JOIN) was passed for any of instream, outstream or
 *  errstream, as appropriate.
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
 * --> pid, @optional
 *  if not NULL, the pid of the exited process (or -1 if no process was
 *  ever forked) is stored in this variable.
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
        const struct string_pair envvars[],
        bool clear_first,
        int instream,
        int outstream,
        int errstream,
        int ms_timeout,
        int *status,
        pid_t *pid,
        process_ioevent_cb ioevent_cb,
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
        const struct string_pair envvars[],
        bool clear_first,
        int instream,
        int outstream,
        int errstream,
        int ms_timeout,
        pid_t *pid,
        process_ioevent_cb ioevent_cb,
        process_completion_cb completion_cb,
        void *priv);


#ifdef __cplusplus
} /* extern "C" */
#endif


#ifdef __cplusplus

#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <optional>


namespace tarp {

class EventPump;

extern "C" {
    void cxx_process_io_callback(
        enum stdStream stream, int fd, unsigned events, void *priv);

    void cxx_process_completion_callback(
            pid_t pid, int status, void *priv);
}

/*
 * Process class that supports:
 *  - synchronous as well as asynchronous spawning of a subprocess
 *  - termination of spawned subprocess on request (for asyncronous processes)
 *  - binding the standard streams of the process to specified descriptors,
 *    to /dev/null/ or to implicitly-created pipes.
 *  - capturing of stdout and/or stderr output to internal buffers, accessible
 *    on process completion. Alternatively, a user callback can be invoked
 *    to do the reading or writing every time there is a file descriptor event.
 *  - automatic termination on specified timeout.
 *
 * NOTE:
 * For information on the meaning of the constructor parameters as well
 * as the synchronous and asynchronous semantics see sync_exec and
 * async_exec above.
 *
 * (1) Constructor for asynchronous process. This may only be used by the
 * event pump (the permit argument enforces this). Clients must instead use
 * the corresponding EventPump method (see tarp/event.hxx). The return is
 * a shared pointer to a Process instance.
 *
 * (2) Constructor for direct instantiation. NOTE Processes created via this
 * constructor may *only* be run synchronosly.
 *
 * (3) run/spawn/execute the subprocess according to its construction and any
 * subsequent configuration (e.g. set_environment). if asynchronous=true,
 * the process is executed asynchronoulsy. The method returns immediately
 * while the process may complete afterward (the user may be notified via
 * the completion callback).
 *
 * Otherwise it is executed synchronously -- the method only returns once
 * the process has completed execution.
 *
 * NOTE: Process created via the EventPump may be run both synchronously
 * and asynchronoulsy. Processes created directly may only be run synchronously.
 *
 * NOTE: any (re)configuration of a running asynchronous process will not take
 * effect until the process has completed (and may then be run again).
 *
 * NOTE: it is illegal to call .run() if the process is running. Ensure the
 * process has terminated before calling .run() again.
 *
 * (4) kill a process immediately. Note this only makes sense for asynchronous
 * processes since a synchronous process by definition will have exited
 * by the time .run() returns.
 *
 * (5) get the pid of the process. Note for a synchronous process this
 * is the pid of the process -- which has *already* exited.  For asynchronous
 * processes the process may or may not have exited. Check 'running()'.
 *
 * (6) get the exit status of a process. This is optional in the sense that
 * a process that has never been .run() or the call to .run returned an error
 * will not have an exit status and the optional class will be empty.
 *
 * (7) true if the process is currently still running.
 *
 * (8) true if the process has exited and was terminated by a signal (for
 * killed on timeout or by something else). False if the process has not
 * exited (still running) or was not terminated by a signal.
 *
 * (9) configure the standard streams of the process so that they are bound
 * to /dev/null. Specifically, set STREAM_ACTION_DEVNULL for all of them.
 *
 * (10) configure the standard streams of the process. See top of file fmi.
 * (11) configure the environment of the process. See top of file fmi.
 *
 * (12) If the user specified NO ioevent_cb but specified STREAM_ACTION_PIPE
 * for any stream, then the process will save the stdout and stder output(s)
 * in their own streams (assuming the streams are separate).
 * If STREAM_ACTION_JOIN is properly specified (see top of file fmi), then
 * both stderr and stdout output are one and the same and will be stored in
 * the stdout output buffer (assuming the subprocess program is not otherwise
 * configured -- e.g. to redirect both stdout and stderr to stderr).
 */
class Process : public std::enable_shared_from_this<tarp::Process>
{
public:
    using ioevent_cb    = std::function<void(enum stdStream stream, int fd, unsigned events)>;
    using completion_cb = std::function<void(std::shared_ptr<tarp::Process>)>;

    friend void cxx_process_io_callback(
        enum stdStream stream, int fd, unsigned events, void *priv);

    friend void cxx_process_completion_callback(
            pid_t pid, int status, void *priv);

    class construction_permit {
    private:
        friend class EventPump;
        friend class Process;
        construction_permit(void) { };
    };

    /* not copiable/clonable or movable. */
    Process(const Process &)               = delete;
    Process(const Process &&)              = delete;
    Process &operator=(const Process &)    = delete;
    Process &operator=(Process &&)         = delete;

    /* (1) */
    Process(
            Process::construction_permit permit,
            std::shared_ptr<tarp::EventPump> evp,
            std::initializer_list<std::string> cmd_spec,
            int ms_timeout = -1,
            int instream  = STREAM_ACTION_PASS,
            int outstream = STREAM_ACTION_PASS,
            int errstream = STREAM_ACTION_PASS,
            ioevent_cb ioevent_callback = nullptr,
            completion_cb completion_callback = nullptr
            );

    /* (2) */
    Process(
            std::initializer_list<std::string> cmd_spec,
            int ms_timeout = -1,
            int instream  = STREAM_ACTION_PASS,
            int outstream = STREAM_ACTION_PASS,
            int errstream = STREAM_ACTION_PASS,
            ioevent_cb ioevent_callback = nullptr
            );

    ~Process(void);

    int run(bool asyncronous = false);   /* (3) */
    void stop(void);                     /* (4) */
    pid_t get_pid(void) const;           /* (5)  */
    std::optional<int> get_exit_code(void) const;  /* (6) */
    bool is_running(void) const;                   /* (7) */
    bool was_killed(void) const;                   /* (8) */
    void close_streams(void);                      /* (9) */

    /* (10) */
    void set_streams(
            int instream = STREAM_ACTION_PASS,
            int oustream = STREAM_ACTION_PASS,
            int errstream = STREAM_ACTION_PASS);

    /* (11) */
    void set_environment(const std::map<std::string, std::string> &env);

    /* (12) */
    std::vector<uint8_t> &outbuff(void);
    std::vector<uint8_t> &errbuff(void);

private:
    int sync_exec( const char **cmd, const struct string_pair *env,  int ms_timeout);
    int async_exec(const char **cmd, const struct string_pair *env, int ms_timeout);
    void complete(int status);
    void clean_state(void);

    std::shared_ptr<tarp::EventPump> m_evp;
    std::vector<std::string> m_cmd_spec;
    std::map<std::string, std::string> m_env_spec;
    pid_t m_pid;
    void *m_raw_pstate;

    int m_stdin_fd;
    int m_stdout_fd;
    int m_stderr_fd;

    bool m_running;
    std::optional<int> m_exit_status;
    int m_timeout;

    /* User callback to invoke when the process has completed (exited/killed) */
    completion_cb m_completion_cb;

    /* User callback to call on any event on any of the streams. See notes
     * above */
    ioevent_cb m_ioevent_cb;

    std::vector<uint8_t> m_outbuff;
    std::vector<uint8_t> m_errbuff;
};

};   /* namespace tarp */

#endif  /* __cplusplus */

#endif   /* TARP_PROCESS_H */
