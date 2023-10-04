#include <string.h>

#include <tarp/common.h>
#include <tarp/bits.h>
#include <tarp/math.h>

bool is_valid_bitstring(const char *s){
    assert(s);
    while (*s){
        if (*s != '1' && *s != '0') return false;
        ++s;
    }
    return true;
}

const char *bitstr(uint64_t val, size_t width){
    size_t msb_pos = posmsb(val);
    size_t strsz = MAX(msb_pos, width) + 1;
    char *s = salloc(sizeof(strsz), NULL);
    char *buff = s;

    assert(buff);
    if (!buff) /* todo, throw error */
        return NULL;

    if (width > msb_pos){
        int padding = width - msb_pos;
        for (int i = 0; i < padding; ++i){
            *buff++ = '0';
        }
    }

    int pos = (msb_pos <= width) ? msb_pos : width;

    while (pos > 0){
        /* number of shifts counted starts from 0 not 1 */
        *buff = (1 & (val >> (pos-1))) ? '1' : '0';
        buff++; pos--;
    }

    return s;
}

int hexstr(
        char dst[],   size_t dstlen,
        uint8_t src[],size_t srclen,
        const char *const delim,
        const char *const prefix)
{
    assert(dst && src);
    const char *const sep  = delim ? delim : "";
    const char *const pref = prefix ? prefix : "";

    /* each byte in src needs space in dst for the separator string, 2 hex digits,
     * Of course, for n bytes the separator is needed only n-1 times. */
    size_t stepsz = strlen(sep) + strlen(pref) + 2 ;

    /* dst must have enough space for the NUL terminator, and the corresponding
     * stepsz for each byte in src */
    size_t totlen = srclen * stepsz + 1 - strlen(sep);

    if (dstlen < totlen){
        return -1;
    }

    memset(dst, 0, dstlen);
    char *d = (char *)dst;

    sprintf(d, "%s%02x", pref, src[0]);
    d += (stepsz - strlen(sep));

    for (size_t i = 1; i < srclen; i++){
        sprintf(d, "%s%s%02x", sep, pref, src[i]);
        d += stepsz;
    }

    return 0;
}

void dump_hex(
    const uint8_t *start,
    uint32_t len,
    uint16_t width,
    bool print_offsets
    )
{
    const uint8_t *p = start;
    for (uint32_t i = 0; i<len; ++i){
        if (i%width==0){
            if (print_offsets){
                fprintf(stderr, "0x%08X | ", i==0 ? 0 : i);
            }
        }

        fprintf(stderr, "0x%02X ", *p++);
        if ((i+1)%width == 0 || i+1 == len){
            fprintf(stderr, "\n");
        }
    }
}

