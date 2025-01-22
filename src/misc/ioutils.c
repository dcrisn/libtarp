#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <unistd.h>

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
