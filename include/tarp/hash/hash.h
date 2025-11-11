#ifndef TARP_HASH_H
#define TARP_HASH_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdint.h>
#include <stdlib.h>


/*
 * Hash functions for use with hash tables
 */

uint64_t jenkins_oaat(const uint8_t *key, size_t len);
uint64_t djb2(const uint8_t key[], size_t len);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
