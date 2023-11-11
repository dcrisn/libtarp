#ifndef TARP_EVENT_FLAGS_H
#define TARP_EVENT_FLAGS_H

/*
 * These flags are meant to be OR-ed into a mask to be passed to Evp_add_fdmon
 * in order to:
 *
 * 1) specify the type of file descriptor event to monitor:
 * -- fd becomes readable or writable.
 *
 * NOTE at least one of FD_EVENT_READABLE or FD_EVENT_WRITABLE must be
 * specified to Evp_add_fdmon.
 *
 * NOTE error conditions on the file descriptor are implicitly monitored.
 * See e.g. man 2 epoll fmi.
 *
 * 2) whether the file descriptor should be made non-blocking before
 * installing a monitor for it
 *
 * 3) whether event notifications are level-triggered (default) or
 * edge-triggered.
 */
#define FD_EVENT_READABLE            1
#define FD_EVENT_WRITABLE            2
#define FD_EVENT_EDGE_TRIGGERED      4
#define FD_NONBLOCKING               8

#endif
