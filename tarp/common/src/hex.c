#include <tarp/hex.h>

void dump_hex(
    const uint8_t *start,
    uint32_t len,
    uint16_t width,
    bool print_offsets
    )
{
  const uint8_t *p = start;
  for (unsigned int i = 0; i<len; ++i){
    if (i%width==0){
      if (print_offsets){
        fprintf(stderr, "0x%04X | ",
            i==0 ? 0 : i);
      }
    }

    fprintf(stderr, "0x%04X ", *p++);

    if ((i+1)%width == 0){
      fprintf(stderr, "\n");
    }
  }
}
