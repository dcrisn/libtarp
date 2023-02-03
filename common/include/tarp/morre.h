/* 
 * morre is mo-err with err spelled backwards.
 * This header file defines error constants to be returned by functions
 * in mythos programs. All programs should use the error constants defined
 * here rather than declare their own, for consistency's sake.
 */

enum morre{
    MORRE_SUCCESS,       /* SUCCESS = 0 */
    MORRE_FULL_RBUFF,    /* ring buffer is full and overwriting is disabled */
    MORRE_EMPTY_RBUFF,   /* ring buffer is empty : nothing to read */
    MORRE_NOROOM,        /* not enough room in a buffer to write */
    MORRE_RBUFF_READ,    /* failed to read from the ring buffer */ 
    MORRE_NOTFOUND,      /* item not found */
};
