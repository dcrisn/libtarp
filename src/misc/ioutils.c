#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include <tarp/error.h>
#include <tarp/ioutils.h>

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
int full_write(int dst, uint8_t *src, int nbytes){
    assert(src);

    int tries         = 0;
    int max_tries     = 5;
    int to_write      = nbytes;
    int bytes_written = 0;

    while(to_write > 0){ /* don't write 0 bytes (implementation-defined) */

        if (tries == max_tries) return ERROR_WRITE;

        bytes_written = write(dst, src, to_write);

        if (bytes_written == -1){
            if (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK){
                ++tries; continue;
            } else return ERROR_WRITE;
        };

        src      += bytes_written;
        to_write -= bytes_written;
        tries = 0;  /* reset tries */

    } /* while write */

    return ERRORCODE_SUCCESS;
}

/*
 * Read buffsz bytes at a time from src until EOF or error and write to dst
 *
 * buff and buffsz refer to a buffer of the specified size provided by
 * the caller to be used internally by this function for carrying out
 * the transfer.
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
int transfer(int src, int dst, uint8_t *buff, size_t buffsz){
    assert(buff);

    int bytes_read = 0;

    while ( (bytes_read = read(src, buff, buffsz)) ){
        if (bytes_read == -1) return ERROR_READ;

        if (full_write(dst, buff, bytes_read) == ERROR_WRITE){
            return ERROR_WRITE;
        }

    }

    return ERRORCODE_SUCCESS;
}


