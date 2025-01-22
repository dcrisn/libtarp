#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <tarp/error.h>
#include <tarp/ioutils.h>

ssize_t try_write(int dst, uint8_t *src, size_t nbytes) {
    assert(src);

    int tries = 0;
    int max_tries = 10;
    int to_write = nbytes;
    ssize_t bytes_written = 0;

    /* don't write 0 bytes (implementation-defined) */
    while (to_write > 0) {
        if (tries == max_tries) {
            break;
        }

        bytes_written = write(dst, src, to_write);

        if (bytes_written != -1) {
            src += bytes_written;
            to_write -= bytes_written;

            // reset the tries counter after each successful write.
            tries = 0;
            continue;
        }

        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }

        // else, -1 AND (EAGAIN or EWOULDBLOCK)
        ++tries;
        pollfd(dst, POLLOUT, 2);
    }  // while write

    return bytes_written;
}

ssize_t try_read(int src, uint8_t *buff, size_t buffsz) {
    assert(src > 0);
    assert(buff != NULL);
    assert(buffsz > 0);

    int tries = 0;
    int max_tries = 10;
    int to_read = buffsz;
    ssize_t bytes_read = 0;

    while (to_read > 0) {
        if (tries == max_tries) {
            break;
        }

        bytes_read = read(src, buff, buffsz);

        if (bytes_read != -1) {
            buff += bytes_read;
            to_read -= bytes_read;

            // reset the tries counter after each successful read.
            tries = 0;
            continue;
        }

        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }

        // else, -1 and EAGAIN or EWOULDBLOCK
        ++tries;
        pollfd(src, POLLIN, 2);
    }  // while read

    return bytes_read;
}

struct pumped pump(int src, int dst, uint8_t *buff, size_t buffsz) {
    assert(buff);

    ssize_t bytes_read = 0;
    ssize_t bytes_written = 0;

    struct pumped ret = {.bytes_read = 0, .bytes_written = 0};

    while ((bytes_read = read(src, buff, buffsz))) {
        if (bytes_read == -1) return ret;

        ret.bytes_read += bytes_read;

        bytes_written = try_write(dst, buff, bytes_read);
        ret.bytes_written += bytes_written;

        if (bytes_written != bytes_read) {
            return ret;
        }
    }

    return ret;
}

int pollfd(int fd, int events, int timeout) {
    if (fd < 0) return fd;

    short evmask = events ? events : POLLIN | POLLOUT;
    const int num_fd = 1;
    int ret = 0;

    struct pollfd pollfd = {.fd = fd, .events = evmask, .revents = 0};

    if ((ret = poll(&pollfd, num_fd, timeout)) > 0) {
        return pollfd.revents; /* success */
    }

    return ret;
}

// socket_fd MUST be non-null.
// path MUST be non-null.
// socket_fd, if successful, will contain an fd ready to call accept() on.
struct result make_server_uds(const char *path,
                              unsigned backlog_len,
                              int *socket_fd,
                              bool stream_type) {
    *socket_fd = -1;

    if (!path) {
        return RESULT(false, "null path argument", 0);
    }

    if (!socket_fd) {
        return RESULT(false, "null socket_fd argument", 0);
    }

    int fd = socket(AF_UNIX, stream_type ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (fd < 0) {
        return RESULT(false, "failed socket() call", errno);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;  // aka AF_LOCAL

    // UDS is named i.e. must be bound to a file in the file system.
    remove(path);
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return RESULT(false, "failed call to bind()", errno);
    }

    if (stream_type && listen(fd, backlog_len) < 0) {
        close(fd);
        return RESULT(false, "failed call to listen()", errno);
    }

    *socket_fd = fd;
    return RESULT(true, "", 0);
}

// socket_fd MUST be non-null.
// path MUST be non-null.
// NOTE: UNIX domain datagram sockets are always reliable and don't reorder
// datagrams). See man 7 unix.
struct result make_client_uds(const char *server_path,
                              const char *client_path,
                              int *socket_fd,
                              bool stream_type) {
    *socket_fd = -1;

    if (!server_path) {
        return RESULT(false, "null server_path argument", 0);
    }

    if (!stream_type && !server_path) {
        return RESULT(false, "null client_path argument", 0);
    }

    if (!socket_fd) {
        return RESULT(false, "null socket_fd argument", 0);
    }

    int fd = socket(AF_UNIX, stream_type ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (fd < 0) {
        return RESULT(false, "failed socket() call", errno);
    }

    struct sockaddr_un srvaddr;
    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sun_family = AF_UNIX;  // aka AF_LOCAL
    strncpy(srvaddr.sun_path, server_path, sizeof(srvaddr.sun_path) - 1);

    // for SOCK_DGRAM the client must bind to a path too.
    if (!stream_type) {
        struct sockaddr_un claddr;
        memset(&claddr, 0, sizeof(claddr));
        claddr.sun_family = AF_UNIX;
        strncpy(claddr.sun_path, client_path, sizeof(claddr.sun_path) - 1);
        remove(client_path);
        if (bind(fd, (const struct sockaddr *)&claddr, sizeof(claddr)) < 0) {
            close(fd);
            return RESULT(false, "failed client bind()", errno);
        }
    }

    if (connect(fd, (const struct sockaddr *)&srvaddr, sizeof(srvaddr)) < 0) {
        unsigned errnum = errno;
        close(fd);
        return RESULT(false, "failed call to connect()", errnum);
    }

    *socket_fd = fd;
    return RESULT(true, "", 0);
}

// Send msg of msgsz to to the uds_dst_fd, and also send the fd file descriptor
// as ancillary data. We assume we are aalready connect()-ed to uds_dst_fd.
//
// No descriptors are closed by this function.
//
// If blocking is true, we try to send all the bytes. Otherwise, on a partial
// write we return to the caller the number of bytes written. The caller can
// then write the rest of the bytes in data, without worrying about ancillary
// data.
struct result send_msg_with_fd(int uds_dst_fd,
                               int fd,
                               uint8_t *msg,
                               size_t msgsz,
                               bool blocking,
                               size_t *num_bytes_written) {
    // Can't send ancillary data without sending
    // at least 1 byte of real data (on Linux).
    if (msgsz == 0 || msg == NULL) {
        return RESULT(false, "empty message", 0);
    }

    // buffer for ancillary data;
    union {
        // space for a single fd -- i.e. a single ancillary data
        // item i.e. a single cmsghdr.
        char buff[CMSG_SPACE(sizeof(int))];

        // the buff is really a cmsghdr so we use a union to
        // ensure proper alignment. See man 3 cmsg.
        struct cmsghdr align;
    } ancillary_data;

    // zero everything to be safe. Note this would actually be required
    // for CMSG_NXTHDR() to work correctly. We only send one ancillary data
    // item in this function though, so we don't use CMSG_NXTHDR().
    memset(&ancillary_data, 0, sizeof(ancillary_data));

    // Where send() takes a data buffer as argument, sendmsg() takes
    // a msghdr struct. This makes it possible to communicate extra
    // options to sendmsg(); such as (as in this case) to attach
    // ancillary data to the user data to be sent.
    struct msghdr msgh;
    memset(&msgh, 0, sizeof(msgh));

    // specify the user data to send.
    struct iovec iov;
    iov.iov_base = msg;
    iov.iov_len = msgsz;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;  // a single iovec

    // specify the ancillary data to send (the fd).
    msgh.msg_control = ancillary_data.buff;
    msgh.msg_controllen = sizeof(ancillary_data.buff);

    if (fd >= 0) {
        //--------------------------
        // Each ancillary data item is stored as a cmsghdr inside
        // the msghdr passed to sendmsg(). We populate that here.
        // We only send one single ancillary data item, so we only
        // need populate the first buffer.
        struct cmsghdr *ancillary_data_buffer = CMSG_FIRSTHDR(&msgh);

        // **aligned** number of bytes required to store the ancillary
        // data (the fd in this case).
        ancillary_data_buffer->cmsg_len = CMSG_LEN(sizeof(int));

        // See man 7 unix. SCM_RIGHTS is specified when open fds are
        // sent; and SOL_SOCKET is a historical holdover.
        ancillary_data_buffer->cmsg_level = SOL_SOCKET;
        ancillary_data_buffer->cmsg_type = SCM_RIGHTS;

        // CMSG_DATA gets us a pointer to the data portion of the
        // ancillary data buffer. We finally populate it.
        memcpy(CMSG_DATA(ancillary_data_buffer), &fd, sizeof(fd));
        //--------------------------

        // printf("Sending FD %d\n", fd);
    }

    /* Finally, send the message. */
    ssize_t ret;

    // We only write ONCE. I.e. we break out of the loop when either:
    // - a complete write has taken place
    // - no write has taken place
    // - a single partial write has taken place
    // We do not want to make a successful call to sendmsg (one that returns
    // something other than -1) more than once, since that would most
    // certaintly wreak havoc w.r.t the ancillary data. A single write (whether
    // partial or not) means (or at least we assume here to mean) that the
    // ancillary data has been sent. If the write was partial, then we then
    // just have to send the rest of the user data.
    int retries = 5;
    do {
        ret = sendmsg(uds_dst_fd, &msgh, 0);
    } while (ret == -1 && retries-- > 0 &&
             (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR));

    if (ret < 0) {
        return RESULT(false, "failed sendmsg", errno);
    }

    if ((size_t)ret == msgsz) {
        return RESULT(true, "", 0);
    }

    if ((size_t)ret > msgsz) {
        return RESULT(false, "BUG: more bytes written than expected (?)", 0);
    }

    // else we have a partial write. Note this is not possible for SOCK_DGRAM
    // sockets. We can only be in this situation (and reach here) if we
    // are dealing with a SOCK_STREAM socket. We send now the rest of the user
    // data (using write()/send() rather than sendmsg(), since we don't want to
    // send the ancillary data again). NOTE (man 2 send):
    // >  The only difference between send() and write(2) is the presence of
    // flags. > With a zero flags argument, send() is equivalent to write(2).
    msg += (size_t)ret;
    msgsz -= (size_t)ret;

    // if requested to block in this function and try to handle partial writes
    if (blocking) {
        ssize_t bytes_written = try_write(uds_dst_fd, msg, msgsz);
        *num_bytes_written = bytes_written;
        if (bytes_written < 0 || (size_t)bytes_written != msgsz) {
            return RESULT(false, "failed partial write", 0);
        }

        return RESULT(true, "", 0);
    }

    // else partial write, but the caller wants to itself handle this scenario
    // (e.g. to write all bytes asynchronously instead of blocking, for
    // example).
    *num_bytes_written = ret;
    return RESULT(true, "", 0);
}

// read at most buffsz bytes the uds_fs into buff; if a descriptor was
// sent along with the data as ancillary data, it is stored in fd.
// This function assumes the descriptor points to
// an already configured socket that a message can be read from.

struct result receive_msg_with_fd(int uds_fd,
                          int *fd,
                          uint8_t *buff,
                          size_t buffsz,
                          size_t min_bytes_to_read,
                          bool blocking,
                          size_t *num_bytes_read) {
    if (buffsz == 0 || buff == NULL) {
        return RESULT(false, "null buffer", 0);
    }

    if (min_bytes_to_read > buffsz) {
        return RESULT(false, "min_bytes_to_read < buffsz", 0);
    }

    // buffer for ancillary data;
    union {
        // space for a single fd -- i.e. a single ancillary data
        // item i.e. a single cmsghdr.
        char buff[CMSG_SPACE(sizeof(int))];

        // the buff is really a cmsghdr so we use a union to
        // ensure proper alignment. See man 3 cmsg.
        struct cmsghdr align;
    } ancillary_data;

    // zero everything to be safe. Note this would actually be required
    // for CMSG_NXTHDR() to work correctly. We only send one ancillary datum
    // in this function though, so we don't use CMSG_NXTHDR().
    memset(&ancillary_data, 0, sizeof(ancillary_data));

    // Where recv() takes a data buffer as argument, recvmsg() takes
    // a msghdr struct. This makes it possible to communicate extra
    // options to recvmsg(); such as (as in this case) to extract
    // ancillary data that might've been sent in the message.
    struct msghdr msgh;
    memset(&msgh, 0, sizeof(msgh));

    // NOTE: the address of the peer socket can be stored in the buffer
    // assigned to msgh.msg_name. We don't need that however, so we
    // leave it as NULL.

    // specify the buffer to receive data into.
    struct iovec iov;
    iov.iov_base = buff;
    iov.iov_len = buffsz;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;  // a single iovec

    int fd_received = -1;

    // specify where to extract the ancillary data (the fd).
    msgh.msg_control = ancillary_data.buff;
    msgh.msg_controllen = sizeof(ancillary_data.buff);

    /* finally, get the normal and ancillary data */

    // We only read ONCE. I.e. we break out of the loop when either:
    // - a complete read (of the entire message) has taken place
    // - no read has taken place
    // - a single partial read has taken place
    // We do not want to make a successful call to recvmsg (one that returns
    // something other than -1) more than once, since that would most
    // certaintly wreak havoc w.r.t the ancillary data. A single read (whether
    // partial or not) means (or at least we assume here to mean) that the
    // ancillary data has been read. If the read was partial, then we then
    // just have to read the rest of the user data.
    int retries = 5;
    ssize_t ret;
    do {
        ret = recvmsg(uds_fd, &msgh, 0);
    } while (ret == -1 && retries-- > 0 &&
             (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR));

    // nothing read.
    if (ret < 0) {
        return RESULT(false, "failed recvmsg", errno);
    }

    // Otherwise we've got a partial read. So we assume we have read the
    // ancillary data and therefore we start by extacting that here.
    // printf("recvmsg() returned %zd\n", ret);

    //--------------------------
    // Each ancillary data item is stored as a cmsghdr inside
    // the msghdr passed to sendmsg().
    // We only expect one single ancillary data item, so we only
    // look at the first buffer.
    struct cmsghdr *ancillary_data_buffer = CMSG_FIRSTHDR(&msgh);

    if (ancillary_data_buffer != NULL) {
        // check if valid. We expect a single fd as ancillary data.
        if (ancillary_data_buffer->cmsg_len != CMSG_LEN(sizeof(int))) {
            const char *e = "failed to extract ancillary data (bad cmsg "
                            "header/message length)";
            return RESULT(false, e, 0);
        }

        // see the send helper equivalent fmi
        if (ancillary_data_buffer->cmsg_level != SOL_SOCKET) {
            const char *e =
              "failed to extract ancillary data (cmsg_level != SOL_SOCKET)";
            return RESULT(false, e, 0);
        }
        if (ancillary_data_buffer->cmsg_type != SCM_RIGHTS) {
            const char *e =
              "failed to extract ancillary data (cmsg_type != SCM_RIGHTS)";
            return RESULT(false, e, 0);
        }

        memcpy(
          &fd_received, CMSG_DATA(ancillary_data_buffer), sizeof(fd_received));
        *fd = fd_received;
        // printf("Received FD %d\n", fd_received);
    }

    *num_bytes_read = ret;

    // read complete, nothing else left to do.
    if ((size_t)ret == buffsz || (size_t)ret >= min_bytes_to_read) {
        return RESULT(true, "", 0);
    }

    if ((size_t)ret > buffsz) {
        return RESULT(false, "BUG: more bytes read than expected (?)", 0);
    }

    // else, handle the partial read. Note this is not possible for
    // SOCK_DGRAM sockets. We can only be in this situation (and reach here)
    // if we are dealing with a SOCK_STREAM socket. We read now the rest of
    // the user data (using read()/recv() rather than sendmsg(), since we
    // don't want to read the ancillary data again). NOTE (man 2 recv): >
    // The only difference between recv() and read(2) is the presence of
    // flags. > With a zero flags argument, recv() is generally equivalent
    // to read(2) > (but see  NOTES)
    buff += (size_t)ret;
    buffsz -= (size_t)ret;
    min_bytes_to_read -= (size_t)ret;

    // if requested to block in this function and try to handle partial
    // reads.
    if (blocking) {
        ssize_t bytes_read = try_read(uds_fd, buff, min_bytes_to_read);
        *num_bytes_read = bytes_read;
        if (bytes_read < 0 || (size_t)bytes_read != min_bytes_to_read) {
            return RESULT(false, "failed partial read", 0);
        }

        return RESULT(true, "", 0);
    }

    // else partial read, but the caller wants to itself handle this
    // scenario (e.g. to read all bytes asynchronously instead of blocking,
    // for example).
    *num_bytes_read = ret;
    return RESULT(true, "", 0);
}
