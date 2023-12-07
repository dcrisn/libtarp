#include <array>
#include <stdexcept>
#include <string>

#include <tarp/common.h>
#include <tarp/log.h>
#include <unistd.h>

#include <tarp/event.hxx>
#include <tarp/process.h>
#include <tarp/types.h>

using namespace std;
using namespace tarp;

/* see process.c */
extern "C"
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
        void *priv);


extern "C"
void async_process_killer(struct timer_event *tev, void *priv);

extern "C"
void tarp::cxx_process_io_callback(
        enum stdStream stream, int fd,
        unsigned events, void *priv)
{
    assert(priv);
    tarp::Process *p = static_cast<tarp::Process *>(priv);

    if (p->m_ioevent_cb){
        p->m_ioevent_cb(stream, fd, events);
        return;
    }

    if (stream == STDIN) return;
    assert(stream == STDOUT || stream == STDERR);

    /* if no user callback is available, simply capture output
     * and store it in an appropriate buffer so it can be consumed
     * once the process has completed. */
    const size_t READ_CHUNK_SZ = 2048;
    auto &buff = (stream == STDOUT) ? p->m_outbuff : p->m_errbuff;

    array<uint8_t, READ_CHUNK_SZ> arr;
    ssize_t bytes_read = read(fd, arr.data(), READ_CHUNK_SZ);

    if (!bytes_read) return;

    if (bytes_read == -1){
        warn("read() error: '%s'", strerror(errno));
        return;
    }

    buff.insert(buff.end(), arr.begin(), arr.begin() + bytes_read);
}

extern "C"
void tarp::cxx_process_completion_callback(pid_t pid, int status, void *priv){
    assert(priv);
    UNUSED(pid);

    tarp::Process *p = static_cast<tarp::Process *>(priv);
    p->complete(status);
}

// return true if pipes will be created internally based on the stream
// action configuration for the std streams of the process.
static inline bool will_create_pipes(int instream, int outstream, int errstream)
{
    if (instream == STREAM_ACTION_PIPE || outstream == STREAM_ACTION_PIPE
            || errstream == STREAM_ACTION_PIPE)
        return true;

    if (outstream == STREAM_ACTION_JOIN && errstream == STREAM_ACTION_JOIN)
        return true;

    return false;
}

// shallow copy; this is only for internal temporary use.
// The vector being converted is a private member outlasting the call
// to this function.
static inline const char **
string_vector_to_c_string_array(const vector<string> &v)
{
    const char **cmd = new const char *[v.size()+1];
    cmd[v.size()] = nullptr;
    for (unsigned i=0; i < v.size(); ++i){
        cmd[i] = v[i].c_str();
    }

    return cmd;
}

// shallow copy; this is only for internal temporary use.
// The map being converted is a private member outlasting the call
// to this function.
static inline string_pair *
string_map_to_c_stringpair_array(const map<string, string> &m)
{
    string_pair *env = new string_pair [m.size()+1];
    env[m.size()].first  = nullptr;
    env[m.size()].second = nullptr;

    int i = 0;
    for (const auto &entry : m){
        env[i++] = {entry.first.c_str(), entry.second.c_str()};
    }

    return env;
}

/*
 * Event Pump shared_ptr
 * --------------------------
 * NOTE that the event pump is captured inside the class as a shared_ptr
 * rather than a weak_ptr. The latter makes a bit more sense semantically
 * speaking but a *running* (asynchronous) process associated with an event pump
 * may not outlast it, otherwise the detructor will call stop(), which will
 * to kill the process using a freed raw evp_handlle pointer in the C API,
 * causing a crash. A synchronous process knows nothing about an EventPump
 * though, so that is safe. An asynchronous process not currently running
 * would not be problematic, at the expense of some additional checks.
 * For simplicity, a full-blown shared pointer is kept. Don't keep processes
 * around after destroying their event pump since they are bound to it.
 *
 * C API
 * -------
 * NOTE that internally the C++ class relies on the C API
 * (sync_exec, async_exec and all the internal code in misc/process.c)
 * to do most of the work.
 * This leads to more memory being used than strictly necessary (for
 * example, more pointers are kept, some file descriptors are stored both
 * in the instance and inside the C API) etc. However, the much worse
 * alternative would be lots of duplication. You're not meant to have lots
 * and lots of processes running at the same time, so this is in practice
 * a non-issue.
 *
 * Another result of this design is that the C API keeps its own raw state
 * (see the following note on m_raw_pstate) that it manages and cleans up
 * when done. The state is associated not with a c++ tarp::Process instance
 * but with an instance of an actually running process. Meaning a state
 * is set up when .run() is called and torn down when the process has
 * terminated and .complete() is called. This may seem a little clumsy but
 * it makes it possible to reuse the C implementation and avoid duplication.
 * It also has the added benefit of simplifying cleanup and logic.
 *
 * m_raw_pstate: this is not to be freed inside the Process class (e.g. in its
 * destructor). It is owned by the C process API and will be freed by it when
 * we return from .complete(). NOTE it is perfectly safe for the user to let the
 * c++ Process instance go out of scope (and therefore cause its destructor to be
 * called and perhaps even the associated event pump to be destructed) from
 * inside the .complete() callback. The C API will have already cleaned up and
 * released all its state by the point .complete() is callled.
 */
Process::Process(
        Process::construction_permit permit,
        std::shared_ptr<tarp::EventPump> evp,
        std::initializer_list<std::string> cmd_spec,
        int ms_timeout,
        int instream,
        int outstream,
        int errstream,
        ioevent_cb ioevent_callback,
        completion_cb completion_callback
        )
    :
        m_evp(evp),
        m_cmd_spec(cmd_spec), m_env_spec(),
        m_pid(-1), m_raw_pstate(nullptr),
        m_stdin_fd(instream), m_stdout_fd(outstream), m_stderr_fd(errstream),
        m_running(false), m_exit_status(),
        m_timeout(ms_timeout),
        m_completion_cb(completion_callback),
        m_ioevent_cb(ioevent_callback),
        m_outbuff(), m_errbuff()
{
    UNUSED(permit); /* only serves as key to allow access to this
                       restricted constructor */
}

Process::Process(
        std::initializer_list<std::string> cmd_spec,
        int ms_timeout,
        int instream,
        int outstream,
        int errstream,
        ioevent_cb ioevent_callback)
    :
        Process(
                Process::construction_permit(), nullptr, cmd_spec, ms_timeout,
                instream, outstream, errstream, ioevent_callback, nullptr)
{}


Process::~Process(void){
    stop();
}

bool Process::was_killed(void) const{
    if (!m_exit_status.has_value())
        return false;

    return m_exit_status.value() == PROC_KILLED;
}

bool Process::is_running(void) const {
    return m_running;
};

pid_t Process::get_pid(void) const {
    return m_pid;
}

std::optional<int> Process::get_exit_code(void) const {
    return m_exit_status;
}

/*
 * NOTE: by definition, a process executed synchronously
 * is no longer running once run() returns. Therefore
 * this function is a NOP for synchronous processes.
 * Similarly a NOP for an asynchronous process no longer running.
 */
void Process::stop(void) {
    if (!is_running()) return;

    assert(m_raw_pstate);
    async_process_killer(nullptr, m_raw_pstate);
}

/*
 * NOTE:
 * This is ony called for asynchronous processes. Since these can
 * only be created through the event pump, the process is ensured to
 * always be a shared_ptr and shared_from_this does not fail.
 *
 * OTOH, for synchronous processes the user may or may not create
 * the process as a shared pointer. If this function were to be called
 * in that case, it could therefore throw an exception. But there is 0
 * benefit to calling a completion callback since the user must wait
 * synchronously for the process to complete and so they can just
 * call whatever code they need to right after the .run() method has returned.
 */
void Process::complete(int status){
    m_exit_status = status;
    m_running = false;

    if (m_completion_cb)
        m_completion_cb(shared_from_this());
}

std::vector<uint8_t> &Process::outbuff(void) {
    return m_outbuff;
}

std::vector<uint8_t> &Process::errbuff(void) {
    return m_errbuff;
}

int Process::sync_exec(const char **cmd, const struct string_pair *env,
        int ms_timeout)
{
   int status, rc;

    process_ioevent_cb io_cb = nullptr;
    if (will_create_pipes(m_stdin_fd, m_stdout_fd, m_stderr_fd)){
        io_cb = cxx_process_io_callback;
    }

    rc = ::sync_exec(cmd, env, true,
                m_stdin_fd, m_stdout_fd, m_stderr_fd,
                ms_timeout, &status, &m_pid, io_cb, this);
    if (rc != 0) return rc;

    m_exit_status = status;
    return 0;
}

int Process::async_exec(const char **cmd, const struct string_pair *env,
        int ms_timeout)
{
    if (!m_evp){
        throw std::logic_error(
                "Asnychronous process must be created through the EventPump api");
    }

    process_ioevent_cb io_cb = nullptr;
    if (will_create_pipes(m_stdin_fd, m_stdout_fd, m_stderr_fd)){
        io_cb = cxx_process_io_callback;
    }

    shared_ptr<RawEventPumpInterface> evp = m_evp;
    int rc = ::async_exec__(evp->get_raw_evp_handle(),
            cmd, env, true,
            m_stdin_fd, m_stdout_fd, m_stderr_fd,
            ms_timeout, &m_pid, &m_raw_pstate,
            io_cb, cxx_process_completion_callback,
            this);

    if (rc == 0) m_running = true;
    return rc;
}

/*
 * When a process run is started, the state related to the previous
 * run (e.g. the stdout and stderr buffer outputs) must be cleaned. */
void Process::clean_state(void){
    m_exit_status.reset();
    m_outbuff.clear();
    m_errbuff.clear();
    m_pid = -1;
    m_raw_pstate = nullptr;
    m_running = false;
}

int Process::run(bool asynchronous){
    if (is_running()) throw std::logic_error(
            "Cannot call .run() on a running process");

    clean_state();

    auto cmd = string_vector_to_c_string_array(m_cmd_spec);
    auto env = string_map_to_c_stringpair_array(m_env_spec);

    int rc;

    if (asynchronous)
        rc = async_exec(cmd, env, m_timeout);
    else
        rc = sync_exec(cmd, env, m_timeout);

    delete[] cmd;
    delete[] env;
    return rc;
}

void Process::close_streams(void) {
    m_stdin_fd = m_stdout_fd = m_stderr_fd = STREAM_ACTION_DEVNULL;
}

void Process::set_streams(int instream, int outstream, int errstream){
    m_stdin_fd = instream;
    m_stdout_fd = outstream;
    m_stderr_fd = errstream;
}

void Process::set_environment(const std::map<std::string, std::string> &env){
    m_env_spec = env;  /* save deep copy */
}

