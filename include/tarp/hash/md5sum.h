#ifndef TARP_MD5SUM_H
#define TARP_MD5SUM_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#define MD5_HASH_LEN  16 /* bytes */

typedef struct md5_context Md5Ctx;

/*
 * Allocate new MD5 context for keeping state across calls to digest a specfic
 * message. Must be detroyed with MD5_destroy(). If digesting multiple messages
 * *in parallel*, they each must have a separate context. IOW, a context is not
 * share-able between multiple messages at one time, but can be reused
 * sequentiallly.
 *
 * The user can specify their own allocator/deallocator for the memory; if they
 * are NULL, the default is used. If specified, priv is whatever pointer the
 * user wants passed to their allocator/deallocator.
 */
Md5Ctx *MD5_newctx(void *(*allocator)(size_t sz, void *), void *priv);
void MD5_destroy(Md5Ctx *ctx, void (*deallocator)(void *data, void *), void *priv);

/*
 * Transfer the message digest from the context to hashbuff, which must be at
 * least 16 bytes wide. This should only be called on completion of MD5
 * processing (i.e. after calling MD5_digest with final=true). */
void MD5_dump(Md5Ctx *ctx, uint8_t hashbuff[16]);

/*
 * Print out a hex-formatted string of the digest stored in hashbuff. */
void MD5_print(uint8_t hashbuff[16]);

/*
 * Digest the specified message.
 *
 * This function can be used to feed input to the algorithm in chunks.
 *  - if 'message' is a part of the whole message to be digested, then call
 *    this with isfinal=false
 *  - if 'message' is the *last* part of the whole message (or indeed the entire
 *    message consists of a single chunk), then call it with isfinal=true.
 * In either case, msglen is the length of the chunk passed to the function,
 * *not* the total length of the whole message (unless it is a single-chunk
 * message). */
void MD5_digest(Md5Ctx *ctx, uint8_t *message, size_t msglen, bool isfinal);

/*
 * Digest the specified string and store the hash output in the given buffer.
 *
 * NOTES:
 *  - The buffer must be at least 16-bytes wide.
 *  - This function is a convenience function that automatically opens a
 *    context, closes it, and makes the required calls to MD5_digest(). Therefore
 *    the user does not need to call Md5_{newctx,destroy,digest} themselves.
 */
int MD5_sdigest(const char *const s, uint8_t digest[16]);

/*
 * Read from the specified file descriptor until EOF and digest the message read.
 *
 * MD5_file_digest allows passing a path-string as the parameter instead of the
 * file descriptor, so the open() call is done for the user.
 *
 * See MD5_sdigest fmi.
 */
int MD5_fdigest(int fd, uint8_t digest[16]);
int MD5_file_digest(const char *const filename, uint8_t digest[16]);



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
