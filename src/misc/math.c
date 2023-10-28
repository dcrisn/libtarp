#include <math.h>

#include <string.h>
#include <tarp/math.h>
#include <tarp/common.h>
#include <tarp/log.h>

#include <tarp/bitarray.h>

#include <stdint.h>
#include <stdlib.h>

/* sieve of Eratosthenes */
void find_primes(size_t limit, char *buff){
    assert(limit > 1);
    assert(buff);

    size_t i, j, factor;

    /* assume all numbers > 1 are prime until proven otherwise */
    memset(buff, 1, limit);

    /* mutiples of i smaller than limit are not prime; put differently,
     * numbers < limit that have a factor other than 1 and themselves
     * are not prime */
    for (i = 2; i*i <= limit; i++){
        if (buff[i]){
            for (j = i; (factor = i*j) < limit; ++j){
                buff[factor] = 0;
            }
        }
    }
}

void dump_primes(size_t limit){
    char *a = salloc(limit, NULL);
    find_primes(limit, a);

    for (size_t i = 1; i < limit; ++i){
        if (a[i]) printf("%zu ", i);
    }
    printf("\n");
}

