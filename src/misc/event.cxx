#include <chrono>
#include <memory>
#include <stdexcept>
#include <sstream>

#include <tarp/common.h>
#include <tarp/error.h>
#include <tarp/log.h>
#include <tarp/timeutils.h>

#include <tarp/event.h>
#include <tarp/event.hxx>

using namespace std;
using namespace tarp;

/* ======================================================
 * ========== Shims for use with the C api ==============
 * =====================================================*/
static void timer_event_callback_shim(struct timer_event *tev, void *priv){
    assert(priv); assert(tev); assert(priv);

    // NOTE this is safe because if the callback had been destructed,
    // we would've unregistered the event in the destructor; hence
    // we're never calling back into freed objects.
    auto *cb = static_cast<tarp::TimerEventCallback *>(priv);
    cb->call();
}

static void fd_event_callback_shim(struct fd_event *fdev, int fd, void *priv){
    assert(priv); assert(fdev); assert(priv);
    UNUSED(fd);

    auto *cb = static_cast<tarp::FdEventCallback*>(priv);
    cb->call();
}

static void user_event_callback_shim(
        struct user_event_watch *uev, unsigned event_type,
        void *data, void *priv)
{
    assert(priv); assert(uev); assert(priv);
    UNUSED(event_type);

    auto *cb = static_cast<tarp::UserEventCallback*>(priv);
    cb->call(data);
}


/*========================================
 *============ EventPump =================
 *=======================================*/
EventPump::EventPump(void)
    : m_callback_id(0), m_callbacks()
{
    m_raw_state = Evp_new();

    if (!m_raw_state){
        ostringstream ss;
        ss << "Failed to initialize event pump: '" << strerror(errno) << "'";
        ss << endl;

        throw std::runtime_error(ss.str());
    }
}

EventPump::~EventPump(void){
    Evp_destroy(&m_raw_state);
}

void EventPump::run(void){
    Evp_run(m_raw_state);
}

int EventPump::push_event(unsigned event_type, void *data){
    return Evp_push_uev(m_raw_state, event_type, data);
}

void EventPump::track_callback(std::shared_ptr<tarp::Callback> callback){
    assert(callback);
    ++m_callback_id;

    /* (on wraparound) skip 0 because that's the sentinel id value
     * callbacks have when not given a tracking id. */
    if (m_callback_id == 0) ++m_callback_id;

    m_callbacks[m_callback_id] = callback;
    callback->set_id(m_callback_id);
}

void EventPump::remove_callback_if_tracked(size_t id){
    size_t num_erased = m_callbacks.erase(id);
    debug("UNTRACKED %zu callback(s) [id = %zu]", num_erased, id);
}

int EventPump::activate_and_track(std::shared_ptr<tarp::Callback> callback){
    int rc = callback->activate();
    if (rc == ERRORCODE_SUCCESS) track_callback(callback);
    return rc;
}

int EventPump::set_timer_callback(
        std::chrono::microseconds interval,
        tarp::timer_callback cb)
{
    return activate_and_track(make_shared<tarp::TimerEventCallback>(
                shared_from_this(), interval, cb));
}

int EventPump::set_fd_callback(int fd, uint32_t flags, tarp::fd_callback cb){
    return activate_and_track(make_shared<tarp::FdEventCallback>(
                shared_from_this(), fd, flags, cb));
}

int EventPump::set_uev_callback(unsigned event_type, tarp::uev_callback cb){
    return activate_and_track(make_shared<tarp::UserEventCallback>(
                shared_from_this(), event_type, cb));
}

struct evp_handle *EventPump::get_raw_evp_handle(void) {
    return m_raw_state;
}

struct evp_handle *Callback::get_raw_evp_handle(std::shared_ptr<tarp::EventPump> evp)
{
    std::shared_ptr<tarp::RawEventPumpInterface> evpump = evp;
    return evpump ? evpump->get_raw_evp_handle() : nullptr;
}

void Callback::remove_callback_if_tracked(std::shared_ptr<tarp::EventPump> evp) const
{
    std::shared_ptr<tarp::RawEventPumpInterface> evpump = evp;
    if (evpump) evpump->remove_callback_if_tracked(get_id());
}

/*================================================
 * ============ TimerEventCallback ===============
 * ==============================================*/
TimerEventCallback::TimerEventCallback(
        std::shared_ptr<tarp::EventPump> evp,
        chrono::microseconds interval,
        tarp::timer_callback cb)
    : CallbackCore<tarp::timer_callback>(evp, cb)
{
    set_interval(interval);
    initialize_raw_event_handle();
}

void TimerEventCallback::set_interval(std::chrono::microseconds interval){
    chrono2timespec(interval, &m_interval, true);
}

/*
 * If still alive, deallocate everything; otherwise assume
 * already dead (.die() must have been called explicitly) */
TimerEventCallback::~TimerEventCallback(void){
    debug("called TimerEventCallback destructor");
    if (!is_dead()) die();
}

void TimerEventCallback::initialize_raw_event_handle(void){
    void *mem = salloc(sizeof(struct timer_event), NULL);
    m_raw_tev_handle = static_cast<struct timer_event *>(mem);

    Evp_init_timer_fromtimespec(
            m_raw_tev_handle, &m_interval, timer_event_callback_shim, this);
}

void TimerEventCallback::destroy_raw_event_handle(void){
    if (m_raw_tev_handle){
        salloc(0, m_raw_tev_handle);
        m_raw_tev_handle = nullptr;
    }
}

int TimerEventCallback::register_event_monitor(struct evp_handle *raw_evp_handle)
{
    assert(raw_evp_handle); assert(m_raw_tev_handle);

    Evp_set_timer_interval_fromtimespec(m_raw_tev_handle, &m_interval);
    return Evp_register_timer(raw_evp_handle, m_raw_tev_handle);
}

int TimerEventCallback::deregister_event_monitor(struct evp_handle *raw_evp_handle)
{
    assert(raw_evp_handle); assert(m_raw_tev_handle);
    Evp_unregister_timer(raw_evp_handle, m_raw_tev_handle);
    return ERRORCODE_SUCCESS;
}

void TimerEventCallback::call(void){
    assert_func_set();

    if (!m_func()){
        die(); return;
    }

    if (activate() != ERRORCODE_SUCCESS){
        throw std::runtime_error("Internal error");
    }
}


/*================================================
 * ============ FdEventCallback =================
 * ==============================================*/
FdEventCallback::FdEventCallback(
        std::shared_ptr<tarp::EventPump> evp,
        int fd, uint32_t flags,
        tarp::fd_callback cb)
    : CallbackCore<tarp::fd_callback>(evp, cb), m_fd(fd), m_flags(flags)
{
    initialize_raw_event_handle();
}

FdEventCallback::~FdEventCallback(void){
    debug("called FdEventCallback destructor");
    if (!is_dead()) die();
}

void FdEventCallback::initialize_raw_event_handle(void){
    void *mem = salloc(sizeof(struct fd_event), NULL);
    m_raw_fdev_handle = static_cast<struct fd_event *>(mem);

    if (Evp_init_fdmon(m_raw_fdev_handle, m_fd, m_flags,
                fd_event_callback_shim, this) != ERRORCODE_SUCCESS)
    {
        throw std::invalid_argument(
            "Failed to initialize fd event with bad flags or descriptor");
    }
}

void FdEventCallback::destroy_raw_event_handle(void){
    if (m_raw_fdev_handle){
        salloc(0, m_raw_fdev_handle);
        m_raw_fdev_handle = nullptr;
    }
}

int FdEventCallback::register_event_monitor(struct evp_handle *raw_evp_handle){
    assert(raw_evp_handle); assert(m_raw_fdev_handle);
    return Evp_register_fdmon(raw_evp_handle, m_raw_fdev_handle);
}

int FdEventCallback::deregister_event_monitor(struct evp_handle *raw_evp_handle)
{
    assert(raw_evp_handle); assert(m_raw_fdev_handle);
    Evp_unregister_fdmon(raw_evp_handle, m_raw_fdev_handle);
    return ERRORCODE_SUCCESS;
}

void FdEventCallback::call(void){
    assert_func_set();
    if (!m_func(m_fd)) die();
}

/*================================================
 * ============ UserEventCallback ================
 * ==============================================*/
UserEventCallback::UserEventCallback(
        std::shared_ptr<tarp::EventPump> evp,
        uint32_t event_type,
        tarp::uev_callback cb)
    :
        CallbackCore<tarp::uev_callback>(evp, cb),
        m_event_type(event_type), m_event_data(nullptr)
{
    initialize_raw_event_handle();
}

UserEventCallback::~UserEventCallback(void){
    debug("called UserEventCallback destructor");
    if (!is_dead()) die();
}

void UserEventCallback::initialize_raw_event_handle(void){
    void *mem = salloc(sizeof(struct user_event_watch), NULL);
    m_raw_uev_handle = static_cast<struct user_event_watch*>(mem);

    int rc = Evp_init_uev_watch(m_raw_uev_handle, m_event_type,
            user_event_callback_shim, this);

    if (rc == ERRORCODE_SUCCESS) return;

    if (rc == ERROR_OUTOFBOUNDS) throw std::invalid_argument(
            "Unacceptable event_type value");

    throw std::runtime_error("Failed to initialize raw user event handle");
}

void UserEventCallback::destroy_raw_event_handle(void){
    if (m_raw_uev_handle){
        salloc(0, m_raw_uev_handle);
        m_raw_uev_handle = nullptr;
    }
}

int UserEventCallback::register_event_monitor(struct evp_handle *raw_evp_handle){
    assert(raw_evp_handle); assert(m_raw_uev_handle);

    return Evp_register_uev_watch(raw_evp_handle, m_raw_uev_handle);
}

int UserEventCallback::deregister_event_monitor(struct evp_handle *raw_evp_handle)
{
    assert(raw_evp_handle); assert(m_raw_uev_handle);
    Evp_unregister_uev_watch(raw_evp_handle, m_raw_uev_handle);
    return ERRORCODE_SUCCESS;
}

void UserEventCallback::call(void){
    assert(m_func);
    if (!m_func(m_event_type, m_event_data)) die();
}

/*
 * TODO passing the data as a void * is very 'C'. Should write an abstract
 * class e.g. UserEventData instead that can be interacted with polymorphically.
 */
void UserEventCallback::call(void *data){
    m_event_data = data;
    call();
}
