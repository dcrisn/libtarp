#ifndef TARP_IOUTILS_H
#define TARP_IOUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

/*
 * Reliably write nbytes from the src buffer to the dst descriptor.
 *
 * This function deals with partial writes which may occur due to
 * e.g. signal interrupts. Unless ERROR_WRITE is returned, then the whole
 * nbytes has been written to dst.
 *
 * It also reattempts the write 5 times in a row when the write fails
 * with one of EINTR, EAGAIN or EWOULDBLOCK. If one of those reattempts
 * actually succeeds, the reattempt counter is reset to 0 (meaning if
 * another error like this were to occur, another 5 reattempts would be
 * carried out). These 3 errors may be considered temporary nonfatal
 * conditions so reattempting a write makes sense.
 *
 * Other errors however are likely fatal so reattempting the write in
 * that case is likely pointless. The function in that case returns ERROR_WRITE,
 * as it's unlikely write() will succeed and there are probably more significant
 * issues with the system anyway.
 *
 * <-- return
 *     ERRORCODE_SUCCESS on success. Else WRITE_FAIL if write() has failed
 *     with an error considered to be fatal or if the maximum number of write
 *     reattempts has been reached without success.
 */
int full_write(int dst, uint8_t *src, int nbytes);

/*
 * Read buffsz bytes at a time from src until EOF or error and write to dst
 *
 * buff and buffsz refer to a buffer of the specified size provided by
 * the caller to be used internally by this function for carrying out
 * the transfer from src to dst.
 *
 * If read() returns:
 * a) -1: transfer() considers it to have failed
 * b) 0 : transfer() considers it to have succeeded but
 *    the internal loop never enters; this is when read
 *    returns EOF. In other words, there is nothing to transfer.
 * c) a certain numbers of bytes that have been read;
 *    In this case the internal loop is entered and write()
 *    will attempt to write that number of bytes to dst.
 *
 * The number of bytes read by `read()` will be written
 * by `write()` in an internal loop, as described above.
 *
 * The full_write() wrapper is used rather than the plain write()
 * in order to attempt recovery from partial writes.
 */
int transfer(int src, int dst, uint8_t *buff, size_t buffsz);

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
 *     *except on success; in that case, rather than returning the number of file
 *      descriptors that are ready (which would be meaningless because there is
 *      only ONE descriptor), the .revents mask associated with that descriptor
 *      is returned instead.
 */
int pollfd(int fd, int events, int timeout);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
