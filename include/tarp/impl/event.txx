#ifndef TARP_EVENT_TPP
#define TARP_EVENT_TPP

template<typename FUNC_TYPE>
CallbackCore<FUNC_TYPE>::CallbackCore(
        std::shared_ptr<tarp::EventPump> evp,
        FUNC_TYPE cb)
    :
    m_func(cb), m_evp(evp), m_isactive(false), m_isdead(false), m_id(0)
{
    if (!m_evp.lock()) throw std::invalid_argument(
        "Illegal attempt to constructor callback from null EventPump");
}

template<typename FUNC_TYPE>
CallbackCore<FUNC_TYPE>::~CallbackCore(void){}

template <typename FUNC_TYPE>
int CallbackCore<FUNC_TYPE>::activate(void){
    int rc = 0;

    /* dead callbacks cannot be activated; new one must be created */
    if (is_dead()) return ERROR_EXPIRED;

    /* already active ? (case handled further up the call chain) */

    /* If the event pump is gone, associated callbacks are unusable */
    auto evp = m_evp.lock();
    if (!evp) return ERROR_EXPIRED;

    assert_func_set();  /* usage or logic error otherwise */

    struct evp_handle *raw = get_raw_evp_handle(evp);
    if (!raw) throw std::logic_error(
            "got NULL raw evp handle from live EventPump");

    if ( (rc = register_event_monitor(raw)) == ERRORCODE_SUCCESS){
        m_isactive = true;
    }
    return rc;
}

template <typename FUNC_TYPE>
void CallbackCore<FUNC_TYPE>::deactivate(void){
    if (!is_active()) return;

    auto evp = m_evp.lock();
    if (!evp) return;

    struct evp_handle *raw = get_raw_evp_handle(evp);
    if (!raw) throw std::logic_error(
            "got NULL raw evp handle from live EventPump");

    deregister_event_monitor(raw);
    m_isactive = false;
}

template <typename FUNC_TYPE>
bool CallbackCore<FUNC_TYPE>::is_active(void) const{
    return m_isactive;
}

template <typename FUNC_TYPE>
void CallbackCore<FUNC_TYPE>::set_func(FUNC_TYPE f){
    m_func = f;
}

template <typename FUNC_TYPE>
void CallbackCore<FUNC_TYPE>::assert_func_set(void) const{
    if (!m_func) throw std::logic_error(
        "Illegal attempt to call or activate empty Callback");
}

template <typename FUNC_TYPE>
void CallbackCore<FUNC_TYPE>::die(void){

    if (is_dead()) return;  /* cannot die twice */

    deactivate();
    destroy_raw_event_handle();
    m_isdead = true;

    /* this triggers the destructor of the Callback object if the EventPump
      has the only/last shared pointer reference to it left. For EventPump-
      managed callbacks, the EventPump has the *only* reference at all times.
      For explicit callbacks, the EventPump has one reference, but normally
      the user lambda shares ownership by capturing the callback object.

      By keeping a shared pointer inside the event pump ('tracking the callback
      object') that's removed when the lambda returns false ('untracking
      the callback object') ensures correct destruction semantics as follows:
       - the callback object does not get destructed before the lambda since the
       event pump keeps it alive. Hence the m_func nullptr assignment below is
       safe. Only the lambda is destructed (unless kept alive by something else).
       - the circular reference between the callback object and the lambda
       (assuming an explicit callback object) is broken here, so a memory
       leak where the 2 keep each other alive is avoided.
       - if the shared pointer inside the event pump removed by untrack_callback
       is the very last reference to the callback object, its destructor is
       invoked. If it is *not* the last reference (the reason being the user
       has other references either to the callback object or the lambda that
       keeps them alive), then the callback object destructor will be invoked
       whenever the other references go out of scope. NOTE this is safe and
       well defined, because by that point die() will have been called,
       unregistering the callback object and breaking the circular dependency
       described above. The Lambda is never called again. There should not be
       any leaks or use-after-free's.
    */
    m_func = nullptr;                /* remove lambda reference */
    untrack_callback(m_evp.lock());  /* remove EventPump Callback object anchor */
}

template <typename FUNC_TYPE>
bool CallbackCore<FUNC_TYPE>::is_dead(void) const{
    return m_isdead;
}

template <typename FUNC_TYPE>
size_t CallbackCore<FUNC_TYPE>::get_id(void) const{
    return m_id;
}

template <typename FUNC_TYPE>
void CallbackCore<FUNC_TYPE>::set_id(size_t id) {
    m_id = id;
}


#endif
