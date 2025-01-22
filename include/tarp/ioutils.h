#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

/*
 * Try to write nbytes from the src buffer to the dst descriptor.
 *
 * This function deals with partial writes which may occur due to
 * e.g. intermittent failures. The number of bytes written is returned.
 * If the result != nbytes, then only a partial write could be effected.
 * The caller can then e.g. call the function again or give up.
 *
 * The function reattempts the write 5 times in a row when the write fails
 * with one of EAGAIN or EWOULDBLOCK. If one of those reattempts
 * actually succeeds, the reattempt counter is reset to 0 (meaning if
 * another error like this were to occur, another 5 reattempts would be
 * carried out). These 3 errors may be considered temporary nonfatal
 * conditions so reattempting a write makes sense.
 *
 * Other errors however are likely fatal so reattempting the write in
 * that case is likely pointless. The function in that case returns the number
 * of bytes written so far, as it's unlikely write() will succeed and there
 * are probably more significant issues with the system anyway.
 *
 * <-- return
 *     the number of bytes written.
 */
ssize_t try_write(int dst, uint8_t *src, size_t nbytes);

/*
 * Try to read buffsz bytes into buff from the file descriptor src.
 * buff must be of size >= buffsz.
 *
 * <-- return
 *     the number of bytes read.
 *
 *  If the return value == buffsz, then the read was complete. Otherwise
 *  the read was partial, and the caller is then free to give up or retry etc.
 *  The function internally retries a number of times when one of EAGAIN
 *  or EWOULDBLOCK are the causes of the partial read, but will give up if
 *  another error occurs. The approach is as described above for try_write.
 */
ssize_t try_read(int src, uint8_t *buff, size_t buffsz);

/*
 * Read buffsz bytes at a time from src until EOF or error and write to dst
 *
 * buff and buffsz refer to a buffer of the specified size provided by
 * the caller to be used internally by this function for carrying out
 * the transfer from src to dst.
 *
 * We assume the following. If read() returns:
 * a) -1: => failed
 * b) 0 : => succeeded but the internal loop never enters; this is when read
 *    returns EOF. In other words, there is nothing to transfer.
 * c) a certain number of bytes that have been read;
 *    In this case the internal loop is entered and write()
 *    will attempt to write that number of bytes to dst.
 *
 * The number of bytes read by `read()` will be written
 * by `write()` in an internal loop, as described above.
 */
struct pumped {
    size_t bytes_read;
    size_t bytes_written;
};

struct pumped pump(int src, int dst, uint8_t *buff, size_t buffsz);

/*
 * Wrapper around the poll() system call for waiting on a single descriptor.
 *
 * For monitoring multiple descriptors, call poll() directly: there is no
 * advantage to having a wrapper.
 *
 * The timeout value must be in milliseconds and will be passed to poll() as is.
 *
 * <-- return
 *     Whatever poll() returns*; errno is also set by poll().
 *
 *     *except on success; in that case, rather than returning the number of
 * file descriptors that are ready (which would be meaningless because there is
 *      only ONE descriptor), the .revents mask associated with that descriptor
 *      is returned instead.
 */
int pollfd(int fd, int events, int timeout);


#ifdef __cplusplus
} /* extern "C" */
#endif
