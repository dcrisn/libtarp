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
CallbackCore<FUNC_TYPE>::~CallbackCore(void){
    m_func = nullptr;
}

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
    debug("called CallbackCore reset");

    deactivate();
    destroy_raw_event_handle();
    m_isdead = true;
    m_func = nullptr;  /* release (lambda) function */

    remove_callback_if_tracked(m_evp.lock());
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
