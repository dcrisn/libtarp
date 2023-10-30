#include <alloca.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <tarp/common.h>
#include <tarp/bits.h>
#include <tarp/bitarray.h>
#include <tarp/log.h>
#include <tarp/error.h>

static const size_t MAX_BITARRAY_WIDTH = SIZE_MAX >> 10;


size_t Bitr_maxcap(void){
    return MAX_BITARRAY_WIDTH;  /* bytes */
}

/*
 * The required size is the number of bytes required to store nbits (which
 * represents the size of the flexible array member) + the sizes of the other
 * struct members.
 *
 * --> nbits
 *  How many bits the array should be big enough to store.
 *
 * --> totsz
 *  If not NULL, the total size (in bytes) that needs to be dynamically
 *  allocated for the struct bitarray will be stored in it.
 *
 * --> bitarray_sz
 * If not NULL, the size required for the bitarray buffer itself, in bytes,
 * will be stored in it.
 */
static void calculate_required_size__(uint32_t nbits, size_t *totsz, size_t *bitarray_sz){
    size_t total_size, buffer_size;

    /* if nbits is not a multiple of 8, an extra byte is needed */
    buffer_size = (nbits / BITS_IN_BYTE) + (nbits % 8 ? 1 : 0);

    total_size = buffer_size +
        membersz(struct bitarray, size) +
        membersz(struct bitarray, width);

    if (totsz) *totsz = total_size;
    if (bitarray_sz) *bitarray_sz = buffer_size;
}

struct bitarray *allocate_bitarray(size_t nbits, void *priv)
{
    assert(nbits > 0);

    size_t totsz, buffsz;
    calculate_required_size__(nbits, &totsz, &buffsz);

    if (buffsz > Bitr_maxcap()) return NULL;

    struct bitarray *new = salloc(totsz, priv);
    new->width = nbits;
    new->size = buffsz;
    return new;
}

static void fill_bitarray__(struct bitarray *bitr, uint8_t byte){
    assert(bitr);
    memset(bitr->bytes, byte, bitr->size);
}

struct bitarray *Bitr_new(size_t nbits, bool one){
    struct bitarray *new = allocate_bitarray(nbits, NULL);
    if (new){
        fill_bitarray__(new, one ? FULL_BYTE : NULL_BYTE);
    }
    return new;
}

void Bitr_destroy(struct bitarray **bitr){
    assert(bitr); /* bitr may not but *bitr may safely be NULL */
    free(*bitr);
    *bitr = NULL;
}

size_t Bitr_width(const struct bitarray *bitr){
    assert(bitr);
    return bitr->width;
}

struct bitarray *Bitr_frombuff(
        struct bitarray *bitr,
        const uint8_t *buffer,
        size_t buffsz)
{
    assert(buffer);
    if (bitr){
        if (bits2bytes(bitr->width, false) < buffsz) return NULL;
    } else{
        if (buffsz > Bitr_maxcap()) return NULL;
        bitr = allocate_bitarray(bytes2bits(buffsz), NULL);
        if (!bitr) return NULL;
    }

    memcpy(bitr->bytes, buffer, buffsz);
    return bitr;
}

/*
 * Create and initialize a bitarray based on a buffer packed, little-endian, with
 * the bytes of val. */
#define generate_unum2bitarray_function(type, postfix) \
    struct bitarray *Bitr_from##postfix(struct bitarray *bitr, type val){ \
        uint8_t buff[sizeof(val)]; \
        pack_buff_le(val, buff);  \
        return Bitr_frombuff(bitr, buff, sizeof(val)); \
    }

/* Bitr_fromu<8,16,32,64> */
generate_unum2bitarray_function(uint8_t, u8)
generate_unum2bitarray_function(uint16_t, u16)
generate_unum2bitarray_function(uint32_t, u32)
generate_unum2bitarray_function(uint64_t, u64)

struct bitarray *Bitr_clone(const struct bitarray *bitr){
    assert(bitr);
    struct bitarray *new = Bitr_frombuff(NULL, bitr->bytes, bitr->size);
    new->size = bitr->size;
    new->width = bitr->width;
    return new;
}

#define generate_bitarray2unum_function(type, postfix) \
    type Bitr_to##postfix(const struct bitarray *bitr){ \
        assert(bitr); \
        assert(bitr->width > 0); \
        type num = 0; \
        for (size_t i = 1; i < bytes2bits(sizeof(type))+1 && i <= bitr->width; ++i){ \
            num =set_bitval(num, i, Bitr_get(bitr, i)); \
        } \
        return num; \
    }

/* Bitr_tou<8,16,32,64> */
generate_bitarray2unum_function(uint8_t, u8)
generate_bitarray2unum_function(uint16_t, u16)
generate_bitarray2unum_function(uint32_t, u32)
generate_bitarray2unum_function(uint64_t, u64)

/*
 * True if 0 < pos <= w, where w is the width of the bitarray, else false.
 *
 * The bits in the bitarray can be thought of as being arranged from index [1]
 * to index [n] (or from [0] to [n-1]).
 */
#define inrange(width, pos)  (pos > 0 && pos <= width)

/* return error if pos is out of bounds;
 * NOTE the error code is negated to make it possible to differentiate between
 * an error and a valid (bit) value */
#define check_position_within_range(width, pos) \
    do { if (!inrange(width, pos)) return -(ERROR_OUTOFBOUNDS); } while (0)

/* Given a 1-based bit position in the bit array, assign:
 * byte_index_var = 0-based byte index inside the byte array buffer
 * bit_index_var = 1-based bit position inside the byte at [byte_index_var]
 */
#define set_indices(pos, byte_index_var, bit_index_var) \
    size_t zero_based_bit_pos__ = pos - 1; \
    size_t byte_index_var = zero_based_bit_pos__ / BITS_IN_BYTE; \
    size_t bit_index_var = (zero_based_bit_pos__ & (BITS_IN_BYTE - 1)) + 1;

/*
 * set bit at specified position to bitval, which *must* be either 1 or 0. */
int Bitr_setval(struct bitarray *bitr, size_t pos, int bitval){
    assert(bitr);
    assert(bitval == 1 || bitval == 0);
    check_position_within_range(Bitr_width(bitr), pos);

    set_indices(pos, byte_idx, bit_pos);
    bitr->bytes[byte_idx] = set_bitval(bitr->bytes[byte_idx], bit_pos, bitval);

    return 0;
}

int Bitr_set(struct bitarray *bitr, size_t pos){
    return Bitr_setval(bitr, pos, 1);
}

int Bitr_clear(struct bitarray *bitr, size_t pos){
    assert(bitr);
    check_position_within_range(Bitr_width(bitr), pos);

    set_indices(pos, byte_idx, bit_pos);
    bitr->bytes[byte_idx] = clear_bit(bitr->bytes[byte_idx], bit_pos);

    return 0;
}

int Bitr_toggle(struct bitarray *bitr, size_t pos){
    assert(bitr);
    check_position_within_range(Bitr_width(bitr), pos);

    set_indices(pos, byte_idx, bit_pos);
    bitr->bytes[byte_idx] = toggle_bit(bitr->bytes[byte_idx], bit_pos);

    return 0;
}

int Bitr_get(const struct bitarray *bitr, size_t pos){
    assert(bitr);
    check_position_within_range(Bitr_width(bitr), pos);

    set_indices(pos, byte_idx, bit_pos);
    return get_bit(bitr->bytes[byte_idx], bit_pos);
}

#define check_position_and_nbits_within_range(pos, nbits) \
    check_position_within_range(pos, nbits); \
    if (nbits > pos) return -(ERROR_OUTOFBOUNDS);

#define do_for_bits(bitfunc, bitr, pos, nbits)\
    assert(bitr); \
    pos = (pos > 0) ? pos : Bitr_width(bitr); \
    nbits = (nbits > 0) ? nbits : pos; /* start of bitarray */; \
    check_position_and_nbits_within_range(pos, nbits); \
    int rc = 0; \
    for (size_t i = pos; i > pos-nbits; --i){ \
        if ( (rc = bitfunc(bitr, i)) != 0) return rc; \
    } \
    return 0

int Bitr_setn(struct bitarray *bitr, uint32_t pos, size_t nbits){
    do_for_bits(Bitr_set, bitr, pos, nbits);
}

int Bitr_clearn(struct bitarray *bitr, uint32_t pos, size_t nbits){
    do_for_bits(Bitr_clear, bitr, pos, nbits);
}

int Bitr_togglen(struct bitarray *bitr, uint32_t pos, size_t nbits){
    do_for_bits(Bitr_toggle, bitr, pos, nbits);
}

bool Bitr_any(const struct bitarray *bitr){
    assert(bitr);
    for (size_t i = 1; i <= bitr->width; ++i){
        if (Bitr_get(bitr, i) == ON_BIT) return true;
    }
    return false;
}

bool Bitr_all(const struct bitarray *bitr){
    assert(bitr);
    for (size_t i = 1; i <= bitr->width; ++i){
        if (Bitr_get(bitr, i) == OFF_BIT) return false;
    }
    return true;
}

bool Bitr_none(const struct bitarray *bitr){
    assert(bitr);
    for (size_t i = 1; i <= bitr->width; ++i){
        if (Bitr_get(bitr, i) == ON_BIT) return false;
    }
    return true;
}

/* do a[i] = (a[i] bop b[i]) for each byte i in a and b, where bop is a bitwise
 * operation */
#define loop_transform_bytes(a, b, bop)  \
    assert(a && b); \
    if (a->width != b->width) return ERROR_INVALIDVALUE; \
    for (size_t i = 0; i < a->size; ++i){ \
        a->bytes[i] bop b->bytes[i];   \
    } \
    return 0;

int Bitr_bor(struct bitarray *a, const struct bitarray *b){
    loop_transform_bytes(a, b, |=);
}

int Bitr_band(struct bitarray *a, const struct bitarray *b){
    loop_transform_bytes(a, b, &=);
}

int Bitr_bxor(struct bitarray *a, const struct bitarray *b){
    loop_transform_bytes(a, b, ^=);
}

int Bitr_bnot(struct bitarray *a){
    loop_transform_bytes(a, a, = ~);
}

struct bitarray *Bitr_slice(const struct bitarray *bitr, size_t start, size_t end){
    assert(bitr);

    if (start == 0) start = 1;
    if (end == 0 ) end = bitr->width+1;

    assert(start < end);
    assert(end <= (bitr->width+1));

    struct bitarray *new = Bitr_new(end - start, false);
    for (size_t i = 0; i < (end - start); ++i){
        Bitr_setval(new, i+1, Bitr_get(bitr, start+i));
    }
    return new;
}

/*
 * NOTE: even when the 'fastpath' does not apply end-to-end, there should still
 * be a certain number of bytes at the beginning that can be memcpy-ed. TODO:
 * try and optimize for those? (same goes for Bitr_join and the like).
 */
struct bitarray *Bitr_repeat(
        const struct bitarray *bitr,
        size_t n
        )
{
    assert(bitr);

    /* the bitarray would have to be grown beyond the max limit;
     * NOTE allocate_bitarray alredy checks for this but the multiplication
     * could overflow and wrap around before it is even called */
    if (Bitr_maxcap()/n < bitr->size) return NULL;

    struct bitarray *new = allocate_bitarray(Bitr_width(bitr) * n, NULL);
    if (!new) return NULL;

    size_t w = Bitr_width(bitr);
    if (w >= BITS_IN_BYTE && ismult2(w)){ /* fastpath, just copy byte ranges */
        for (unsigned int i = 0; i < n; i++){
            memcpy(new->bytes+(bitr->size*i), bitr->bytes, bitr->size);
        }
    } else { /* slowpath -- do it bit by bit */
        for (size_t round = 0; round < n; ++round){
            for (size_t i = 1; i <= bitr->width; ++i){
                Bitr_setval(new, (bitr->width * round)+i, Bitr_get(bitr, i));
            }
        }
    }
    return new;
}

struct bitarray *Bitr_join(struct bitarray *a, const struct bitarray *b){
    assert(a); assert(b);

    if ((Bitr_maxcap() - a->size) < b->size)
        return NULL;

    struct bitarray *new = allocate_bitarray(Bitr_width(a) + Bitr_width(b), NULL);
    size_t wa = Bitr_width(a);
    size_t wb = Bitr_width(b);

    /* fastpath */
    if ((wa>=BITS_IN_BYTE && ismult2(wa)) && (wb>=BITS_IN_BYTE && ismult2(wb))){
        memcpy(new->bytes, b->bytes, b->size);
        memcpy(new->bytes + b->size, a->bytes, a->size);
    } else {  /* slowpath */
        for (size_t i = 1; i <= b->width; ++i){
            Bitr_setval(new, i, Bitr_get(b, i));
        }
        for (size_t i = 1; i <= a->width; ++i){
            Bitr_setval(new, b->width+i, Bitr_get(a, i));
        }
    }
    return new;
}

char *Bitr_tostring(struct bitarray *bitr, size_t split_every, const char *sep)
{
    assert(bitr);
    sep = sep ? sep : "";
    size_t seplen = strlen(sep);
    size_t groups = 0, groupsz = split_every;

    /* + NULL terminator */
    size_t totsz = bitr->width + 1;

    if (groupsz > 0 && groupsz < bitr->width){
        assert(sep);

        /* if n = width/groupsz, the separator must be inserted n-1 times if
         * width is a multiple of groupsz and (n-1)+1=n times if it is not. */
        size_t separators = (bitr->width / groupsz) -
            (bitr->width % groupsz ? 0 : 1);

        groups = separators+1;
        totsz += strlen(sep) * separators;
    }

    char *s = salloc(totsz, NULL);
    char *ptr = s + (totsz-2); /* position before NULL */

    for (size_t i = 1; i <= bitr->width; ++i){
        *ptr-- = (Bitr_get(bitr, i) == ON_BIT) ? '1' : '0';
        if (groups > 1 && (i % groupsz) == 0){
            if (--groups){ /* don't put separator after last group */
                assert(seplen > 0);
                ptr -= seplen;
                /* NOTE the pointer has been decremented by seplen +1! */
                memcpy(ptr+1, sep, seplen);
            }
        }
    }

    return s;
}

/*
 * Compare byte by byte instead of bit by bit, for speed. */
bool Bitr_equal(const struct bitarray *a, const struct bitarray *b){
    assert(a && b);
    if (a->width != b->width){
        return false;
    }

    assert(a->size == b->size);
    size_t i = 0;
    size_t remainder_bits = a->width % 8;

    for (; i < (bits2bytes(a->width, false)); ++i){
        if (a->bytes[i] != b->bytes[i]) return false;
    }

    /* last byte may be different if the width is not a byte multiple as in that
     * case there could be uninitialized bits or bits initialized differently.
     * Consider the following scenario:
     *   - a = initialized to width 7, all 1s
     *   - b = initialized to width 7, all 0s
     *   - b = toggle all its bits (the 7 of them).
     *   - compare a and b. They will *not* be equal because bit 8 is different.
     */
    uint8_t a_byte = a->bytes[i];
    uint8_t b_byte = b->bytes[i];
    if (remainder_bits > 0){
        for (size_t j = 1; j <= remainder_bits; ++j){
            if (get_bit(a_byte, j) != get_bit(b_byte, j)){
                return false;
            }
        }
        return true;
    }

    return (a_byte == b_byte);
}

struct bitarray *Bitr_reverse(struct bitarray *bitr){
    assert(bitr);
    for (size_t i = 1, j = bitr->width; i < j; i++, j--){
        int temp = Bitr_get(bitr, i);
        Bitr_setval(bitr, i, Bitr_get(bitr, j));
        Bitr_setval(bitr, j, temp);
    }
    return bitr;
}

void Bitr_lshift(struct bitarray *bitr, size_t n, bool rotate){
    assert(bitr);
    assert(n <= bitr->width);
    if (n == 0) return; /* nothing to do */

    /* to temporarily store bits that get shifted off one end */
    struct bitarray *slice = rotate ? Bitr_slice(bitr, (bitr->width+1)-n, 0) : NULL;

    /* move the bits n positions to the lelft, then clear then n least
     * significant bits */
    for (size_t i = bitr->width; i > n; i--){
        Bitr_setval(bitr, i, Bitr_get(bitr, i-n));
    }

    /* if rotational, add back the bits that got shifted off at the other end */
    if (rotate){
        for (size_t i = 1; i <= slice->width; ++i){
            Bitr_setval(bitr, i, Bitr_get(slice, i));
        }
    } else { /* else clear the now-garbage bits */
        Bitr_clearn(bitr, n, n);
    }

    Bitr_destroy(&slice);
}

void Bitr_rshift(struct bitarray *bitr, size_t n, bool rotate){
    assert(bitr);
    assert(n <= bitr->width);
    if (n == 0) return; /* nothing to do */

    /* to temporarily store bits that get shifted off one end */
    struct bitarray *slice = rotate ? Bitr_slice(bitr, 1, n+1) : NULL;

    /* move the bits n positions to the right, then clear then most
     * significant bits */
    for (size_t i = 1; i <= (bitr->width-n); i++){
        Bitr_setval(bitr, i, Bitr_get(bitr, i+n));
    }

    /* if rotational, add back the bits that got shifted off at the other end */
    if (rotate){
        size_t start = bitr->width - n;
        for (size_t i = 1; i <= slice->width; ++i){
            Bitr_setval(bitr, start+i, Bitr_get(slice, i));
        }
    } else { /* else clear the now-garbage bits */
        Bitr_clearn(bitr, bitr->width, n);
    }

    Bitr_destroy(&slice);
}

void Bitr_lpush(struct bitarray *bitr, bool on){
    Bitr_rshift(bitr, 1, false);
    Bitr_setval(bitr, Bitr_width(bitr), on ? 1 : 0);
}

void Bitr_rpush(struct bitarray *bitr, bool on){
    Bitr_lshift(bitr, 1, false);
    Bitr_setval(bitr, 1, on ? 1 : 0);
}

int Bitr_lpop(struct bitarray *bitr){
    int bit = Bitr_get(bitr, Bitr_width(bitr));
    Bitr_lshift(bitr, 1, false);
    return bit;
}

int Bitr_rpop(struct bitarray *bitr){
    int bit = Bitr_get(bitr, 1);
    Bitr_rshift(bitr, 1, false);
    return bit;
}

struct bitarray *Bitr_fromstring(struct bitarray *bitr,
        const char *bitstring, const char *sep)
{
    assert(bitstring);
    size_t len = strlen(bitstring);
    size_t true_len = 0;
    if (!is_valid_bitstring(bitstring, sep, &true_len)) return NULL;

    if (bitr){
        if (bitr->width < true_len) return NULL;
    }else{
        bitr = allocate_bitarray(true_len, NULL);
        if (!bitr) return NULL;
    }

    /* bsi = bitstring index; bai = bitarray index */
    for (size_t bsi = len, bai = 1; bsi > 0 && bai <= true_len; --bsi){
        size_t bsidx=bsi-1, bschar=bitstring[bsidx];
        if (bschar == '1' || bschar == '0'){
            Bitr_setval(bitr, bai++, (bschar == '1') ? 1 : 0);
        }
    }

    return bitr;
}

