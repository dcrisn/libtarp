#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <endian.h>

#include <tarp/math.h>
#include <tarp/common.h>
#include <tarp/bits.h>
#include <tarp/hash/md5sum.h>

/*
 * References:
 *  - RFC1321
 */

#define MD5_BLOCKSZ               64  /* bytes */
#define MD5_MODULUS               512
#define MD5_CONGRUENCE_CONSTANT   448

define_modulo_pow2(unsigned int, 8, 8)

struct md5_context {
    /* hash as it gets computed; these are the MD5 'registers' a,b,c,d -
     * - see (RFC1321/3.3) */
    uint32_t hash[4];

    /* block to be processed; data is copied here so as not to change user's
     * buffer. The bytes here at the time of processing must be arranged as if
     * they are a sequence of uint32_ts decoded from little endian */
    uint8_t block[MD5_BLOCKSZ];

    /* where in the block we are; can and will process whole blocks only */
    size_t cursor;

    /* track total length of user's input, even when fed piecemeal */
    uint64_t input_len;
   };

Md5Ctx *MD5_newctx(void *(*allocator)(size_t, void *), void *priv){
    size_t sz = sizeof(Md5Ctx);
    Md5Ctx *ctx = allocator ? allocator(sz, priv) : salloc(sz, NULL);

    assert(ctx);
    if (!ctx) return NULL;

    ctx->cursor    = 0;
    ctx->input_len = 0;

    /* "MD5 registers", initialized to magic values; see RFC1321/3.3*/
    ctx->hash[0] = 0x67452301; /* A */
    ctx->hash[1] = 0xefcdab89; /* B */
    ctx->hash[2] = 0x98badcfe; /* C */
    ctx->hash[3] = 0x10325476; /* D */

    return ctx;
}

void MD5_destroy(Md5Ctx *ctx, void (*deallocator)(void *, void *), void *priv){
    assert(ctx);

    if (deallocator){
        deallocator(ctx, priv);
    }else{
        salloc(0, ctx);
    }
}

void MD5_dump(Md5Ctx *ctx, uint8_t hashbuff[16]){
    assert(ctx);

    uint32_t *buff = (uint32_t *) hashbuff;
    for (unsigned int i = 0; i < ARRLEN(ctx->hash); ++i){
        buff[i] = htole32(ctx->hash[i]);
    }
}

/*
 * RFC1321/3.1.
 * The message is "padded" (extended) so that its length (in bits) is
 * congruent to 448, modulo 512. That is, the message is extended so
 * that it is just 64 bits shy of being a multiple of 512 bits long.
 */
static inline size_t find_padding(int len){
    /* calculate it in bytes rather than bits */
    return (find_congruent_value(\
                len, MD5_CONGRUENCE_CONSTANT>>3, MD5_MODULUS>>3) - len);
}

/*
 * Dynamically allocate a buffer and populate it with:
 * 1) the padding to follow the preceding message
 * 2) 8 btes (a uint64) encoding the total length of preceding message
 * *in bits*.
 */
static void *get_trailer(Md5Ctx *ctx, uint64_t *outlen){
    int len = ctx->cursor; /* we have to pad from here */

    /* @RFC1321/3.1:
     * Padding is always performed, even if the length of the message is already
     * congruent to 448, modulo 512 ... In all, at least one bit and at most 512
     * bits are appended. */
    int padding = find_padding(len);
    int trailersz = padding + 8;
    //printf("(1) padding needed for %i is %d; trailersz=%i\n", len, padding, trailersz);
    uint8_t *trailer=salloc(trailersz, NULL);

    /* the padding must be initialized with a 1 then all 0s, @RFC1321/3.1 */
    trailer[0] = 0x80;
    for (int i = 1; i < (trailersz - 8); ++i){
        trailer[i] = 0;
    }

    /* ... then append a u64 of the original length *in bits*, @RFC1321/3.2 */
    uint64_t l = htole64(ctx->input_len<<3);
    memcpy(trailer+(trailersz-8), &l, sizeof(uint64_t));
    *outlen=trailersz;
    return trailer;
}

/*
 * Before byte sequence in the block must be split up into 4byte words and each
 * word must be interpreted as if it were stored little endian (see RFC1321/2).
 * This function performs little-endian to host decoding of each word in the
 * byte sequence that makes up the context block.
 */
static void decode_block_le32toh(Md5Ctx *ctx){
    /* LE to host decoding */
    for (int i = 0; i < MD5_BLOCKSZ; i+=4){
        uint32_t word = le32toh( *((uint32_t *)(&ctx->block[i])) );
        memcpy(&ctx->block[i], &word, sizeof(uint32_t));
    }
}

/*
 * @RFC1321:
 * the sin values are obtained as follows:
 * ... a 64-element table T[1 ... 64] constructed from the
 * sine function. Let T[i] denote the i-th element of the table, which
 * is equal to the integer part of 4294967296 times abs(sin(i)), where i
 * is in radians.
 */
struct input_triple {
    uint8_t  idx;	/* index into input data block */
    uint8_t shift;	/* amount to shift-rotate left by */
    uint32_t sinv;	/* integer part of 4294967296 times abs(sin(i)) */
} transformation_inputs[] = {
	/* round 1 */
	{0, 7,   0xd76aa478},
	{1, 12,  0xe8c7b756},
	{2, 17,  0x242070db},
	{3, 22,  0xc1bdceee},
	{4, 7,   0xf57c0faf},
	{5, 12,  0x4787c62a},
	{6, 17,  0xa8304613},
	{7, 22,  0xfd469501},
	{8, 7,   0x698098d8},
	{9, 12,  0x8b44f7af},
	{10, 17, 0xffff5bb1},
	{11, 22, 0x895cd7be},
	{12, 7,  0x6b901122},
	{13, 12, 0xfd987193},
	{14, 17, 0xa679438e},
	{15, 22, 0x49b40821},

	/* round 2 */
	{1,  5,  0xf61e2562},
	{6,  9,  0xc040b340},
	{11, 14, 0x265e5a51},
	{0,  20, 0xe9b6c7aa},
	{5,  5,  0xd62f105d},
	{10, 9,  0x2441453 },
	{15, 14, 0xd8a1e681},
	{4,  20, 0xe7d3fbc8},
	{9,  5,  0x21e1cde6},
	{14, 9,  0xc33707d6},
	{3,  14, 0xf4d50d87},
	{8,  20, 0x455a14ed},
	{13, 5,  0xa9e3e905},
	{2,  9,  0xfcefa3f8},
	{7,  14, 0x676f02d9},
	{12, 20, 0x8d2a4c8a},

	/* round 3 */
	{5,  4,  0xfffa3942},
	{8,  11, 0x8771f681},
	{11, 16, 0x6d9d6122},
	{14, 23, 0xfde5380c},
	{1,  4,  0xa4beea44},
	{4,  11, 0x4bdecfa9},
	{7,  16, 0xf6bb4b60},
	{10, 23, 0xbebfbc70},
	{13, 4,  0x289b7ec6},
	{0,  11, 0xeaa127fa},
	{3,  16, 0xd4ef3085},
	{6,  23, 0x4881d05 },
	{9,  4,  0xd9d4d039},
	{12, 11, 0xe6db99e5},
	{15, 16, 0x1fa27cf8},
	{2,  23, 0xc4ac5665},

	/* round 4 */
	{0,  6,  0xf4292244},
	{7,  10, 0x432aff97},
	{14, 15, 0xab9423a7},
	{5,  21, 0xfc93a039},
	{12, 6,  0x655b59c3},
	{3,  10, 0x8f0ccc92},
	{10, 15, 0xffeff47d},
	{1,  21, 0x85845dd1},
	{8,  6,  0x6fa87e4f},
	{15, 10, 0xfe2ce6e0},
	{6,  15, 0xa3014314},
	{13, 21, 0x4e0811a1},
	{4,  6,  0xf7537e82},
	{11, 10, 0xbd3af235},
	{2,  15, 0x2ad7d2bb},
	{9,  21, 0xeb86d391}
};

/*
 * Process the current block in the context. This is to be called only when a
 * whole block has been populated (64 btyes) */
static void MD5_process_block(Md5Ctx *ctx){

    /* save MD5 registers */
    uint32_t a = ctx->hash[0];
    uint32_t b = ctx->hash[1];
    uint32_t c = ctx->hash[2];
    uint32_t d = ctx->hash[3];

    decode_block_le32toh(ctx);

    struct input_triple *t = transformation_inputs;
    uint32_t tmp;

    for(int i = 0; i < 64; i++, t++){
        /* See RFC1321/3.4.
         * The processing for a 64-byte block is split up into 4 different
         * 'rounds' of 16 steps each.
         * 1) at each round, a different transformation function is applied.
         * 2) at each step, the md5 'registers' (a,b,c,d) are rotated as to
         *    the order they are passed as inputs to the transformation function.
         */
        switch(i>>4){
        case 0:
            a += (b & c) | (~b & d);      /* F() */
            break;
        case 1:
        	a += (b & d) | (c & ~d);      /* G() */
        	break;
        case 2:
        	a += b ^ c ^ d;              /* H() */
        	break;
        case 3:
        	a += c ^ (b | ~d);           /* I() */
        	break;
        }

        a += ((uint32_t *)ctx->block)[t->idx] + t->sinv;
        a = lrotu32(a, t->shift);  /* the shift is *rotational* (RFC1321/2) */
        a += b;

        /* rotate variables */
        tmp =  d;
        d   =  c;
        c   =  b;
        b   =  a;
        a   =  tmp;
    }

    /* add in the results of the transformation rounds */
    ctx->hash[0] += a;
    ctx->hash[1] += b;
    ctx->hash[2] += c;
    ctx->hash[3] += d;
}

/*
 * Digest a chunk that represents a part of an entire message consumed
 * piecemeal. When a whole block has been filled in the context, it gets
 * processed. */
static void MD5_digest_part(Md5Ctx *ctx, uint8_t *message, uint64_t msglen){
    int pos = ctx->cursor;
    uint8_t *block = ctx->block+pos;
    uint8_t *eom = message+msglen; /* end of message */
    size_t bytes = 0;

    /* this was initially a much simpler loop; it has been rewritten to use
     * memcpy however - which is heavily optimized and under stress conditions
     * (massive files) this loop alone can save a few seconds */
    while (message < eom){
        /* copy enough bytes to either fill the context block, or to finish off
         * the message, whichever is first */
        bytes = MIN((MD5_BLOCKSZ - pos), (eom - message));
        memcpy(block, message, bytes);

        pos+=bytes;
        block+=bytes;
        message+=bytes;

        if (pos & MD5_BLOCKSZ){
            MD5_process_block(ctx);
            pos = 0;
            block = ctx->block;
        }
    }
    ctx->cursor = pos;
}

/*
 * Digest either a part of a message or an entire message.
 * @param message: the entire message to digest, or a part thereof
 * @param msglen:   the length of 'message' (the chunk, not the accumulated size)
 * @param isfinal: if 'message' is the entire message or the *last* chunk of a
 *                 a larger message, isfinal should be True; otherwise False.
 */
void MD5_digest(Md5Ctx *ctx, uint8_t *message, size_t msglen, bool isfinal)
{
    if (msglen > 0){
        MD5_digest_part(ctx, message, msglen);
        ctx->input_len += msglen;
    }

    if (isfinal){
        message = get_trailer(ctx, &msglen);
        MD5_digest_part(ctx, message, msglen);
        free(message);
    }
}

int MD5_fdigest(int fd, uint8_t digest[16]){
    Md5Ctx *ctx = MD5_newctx(NULL, NULL);

    uint8_t buffer[MD5_BLOCKSZ<<8] = {0};
    int bytes_read = 0;

    while ( (bytes_read = read(fd, buffer, MD5_BLOCKSZ<<8)) > 0 ){
        MD5_digest(ctx, buffer, bytes_read, false);
    }

    if (bytes_read < 0){
        MD5_destroy(ctx, NULL, NULL);
        return errno;
    }

    MD5_digest(ctx, buffer, 0, true);
    MD5_dump(ctx, digest);
    MD5_destroy(ctx, NULL, NULL);
    return 0;
}

int MD5_file_digest(const char *const filename, uint8_t digest[16]){
    int err = 0; errno = 0;

    int fd = open(filename, O_RDONLY);
    if (fd < 0){
        fprintf(stderr, "Failed to open '%s': '%s'", filename, strerror(errno));
        return errno;
    }

    err = MD5_fdigest(fd, digest);
    close(fd);
    return err;
}

int MD5_sdigest(const char *const s, uint8_t digest[16]){
    assert(s);
    Md5Ctx *ctx = MD5_newctx(NULL, NULL);
    MD5_digest(ctx, (uint8_t *)s, strlen(s), false);
    MD5_digest(ctx, NULL, 0, true);
    MD5_dump(ctx, digest);
    MD5_destroy(ctx, NULL, NULL);

    return 0;
}

void MD5_print(uint8_t buff[16]){
    printf("%02x%02x%02x%02x"
           "%02x%02x%02x%02x"
           "%02x%02x%02x%02x"
           "%02x%02x%02x%02x",
            buff[0], buff[1], buff[2], buff[3],
            buff[4], buff[5], buff[6], buff[7],
            buff[8], buff[9], buff[10],buff[11],
            buff[12],buff[13],buff[14],buff[15]
            );
}
