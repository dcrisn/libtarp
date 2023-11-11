#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include <tarp/error.h>
#include <tarp/ioutils.h>

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


