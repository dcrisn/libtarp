#ifndef TARP_EVENT_IMPL_H
#define TARP_EVENT_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

struct timer_event {
    struct dlnode link;
    bool registered;
    struct timespec tspec;
    timer_callback cb;
    void *priv;
};

struct fd_event {
    struct dlnode link;
    bool registered;
    int fd;
    uint32_t evmask;   /* events of interest */
    uint32_t revents;  /* events that have occured (returned by OS) */
    fd_event_callback cb;
    void *priv;
};

struct user_event_watch {
    void *priv;
    user_event_callback cb;
    unsigned event_type;
    bool registered;
};


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif
