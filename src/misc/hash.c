#include <endian.h>
#include <tarp/common.h>
#include <tarp/hash/checksum/md5sum.h>
#include <tarp/hash/hash.h>

#include <stdint.h>

/*
 * Jenkins, one at a time */
uint64_t jenkins_oaat(const uint8_t key[], size_t len){
    assert(key); assert(len > 0);
    size_t i = 0;
    uint64_t hash = 0;

    while (i < len) {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

uint64_t djb2(const uint8_t key[], size_t len){
    uint64_t hash = 5381;
    size_t i = 0;
    int c;

    while (i < len){
        c = key[i];
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        ++i;
    }

    return hash;
}

