#include <sys/epoll.h>
#include <fcntl.h>
#include <tarp/error.h>
#include <unistd.h>
#include <errno.h>

#include <tarp/dllist.h>
#include <tarp/common.h>
#include <tarp/log.h>

#include <tarp/event.h>
#include "event_shared_defs.h"


#ifdef __linux__

#define MAX_EPOLL_BATCH 50

struct os_event_api_handle {
    int epoll_handle;
};


struct os_event_api_handle *get_os_api_handle(void){
    struct os_event_api_handle *handle = salloc(sizeof(struct os_event_api_handle), NULL);
    int rc = epoll_create1(EPOLL_CLOEXEC);
    if (rc < 0){
        error("epoll_create1 error: '%s'", strerror(errno));
        salloc(0, handle);
        return NULL;
    }

    handle->epoll_handle = rc;
    return handle;
}

void destroy_os_api_handle(struct os_event_api_handle *os_api_handle){
    if (!os_api_handle) return;

    close(os_api_handle->epoll_handle);
    salloc(0, os_api_handle);
}

int add_fd_event_monitor(
        struct os_event_api_handle *os_api_handle,
        struct fd_event *fdev)
{
    assert(fdev);
    assert(fdev->fd >= 0);

    uint32_t mask = 0;
    if (fdev->evmask & FD_EVENT_READABLE)       mask |= EPOLLIN | EPOLLRDHUP;
    if (fdev->evmask & FD_EVENT_WRITABLE)       mask |= EPOLLOUT;
    if (fdev->evmask & FD_EVENT_EDGE_TRIGGERED) mask |= EPOLLET;

    int epollfd = os_api_handle->epoll_handle;
    struct epoll_event e = {
        .events = mask,
        .data.ptr = fdev
    };

    int rc = epoll_ctl(epollfd,
            fdev->registered ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
            fdev->fd, &e);

    if (rc < 0){
        error("epoll_ctl error: '%s'", strerror(errno));
        return -1;
    }

    return 0;
}

int remove_fd_event_monitor(
        struct os_event_api_handle *os_api_handle,
        struct fd_event *fdev)
{
    assert(os_api_handle);
    assert(fdev);

    int epollfd = os_api_handle->epoll_handle;
    struct epoll_event e; /* avoid bugs on old kernels */
    int rc = epoll_ctl(epollfd, EPOLL_CTL_DEL, fdev->fd, &e);
    if (rc < 0) error("epoll_ctl(EPOLL_CTL_DEL) error: '%s'", strerror(errno));

    return 0;
}

int pump_os_events(struct evp_handle *handle){
    assert(handle);
    int epollfd = handle->osapi->epoll_handle;

    struct epoll_event buff[MAX_EPOLL_BATCH];

    /* Will unblock when the timerfd (see main event pump loop) or some
     * other event fires, whichever happens first */
    int rc = epoll_wait(epollfd, buff, MAX_EPOLL_BATCH, -1);
    if (rc < 0){
        error("epoll_wait error: '%s'", strerror(errno));
        return -1;
    }

    struct fd_event *fdev;
    struct epoll_event *ev = buff;

    for (int i = 0; i < rc; ++i, ev+=i){
        fdev = ev->data.ptr;
        Dll_pushback(&handle->evq, fdev, link);
    }

    return 0;
}


#endif  /* __linux__ */

