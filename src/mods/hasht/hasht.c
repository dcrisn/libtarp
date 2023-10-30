#include <assert.h>
#include <tarp/common.h>
#include <tarp/error.h>
#include <tarp/bits.h>
#include <tarp/hash/hash.h>

#include <tarp/hasht.h>

/*
 * HASHT_MAX_COUNT should *at least* be sufficiently smaller than SIZE_MAX
 * such that you can multiply it by 100 without overflow (see calculation
 * for the load factor). In practice, it's reasonable to make it much lower.
 */
const size_t HASHT_MAX_MEM      =         SIZE_MAX >> 4;
const size_t HASHT_MAX_BUCKETS  =         HASHT_MAX_MEM / sizeof(struct hashtnode);
const size_t HASHT_MAX_COUNT    =         SIZE_MAX / 1000;
#define      HASHT_MIN_BUCKETS            16
#define      HASHT_DEFAULT_LOAD_FACTOR    80    /* 0.8 */

static hashf HASHT_DEFAULT_HASHFUNC = djb2;


size_t Hasht_maxcap(void){
    assert(HASHT_MAX_COUNT >= HASHT_MAX_BUCKETS);
    return HASHT_MAX_COUNT;
}

// if ht = NULL, then alloc ht as well, else only allocate the buckets vector
struct hasht *allocate_hasht__(struct hasht *ht, size_t num_buckets){
    if (num_buckets < HASHT_MIN_BUCKETS) num_buckets = HASHT_MIN_BUCKETS;

    if (num_buckets > HASHT_MAX_BUCKETS){
        debug("vector of excessive width %zu requested", num_buckets);
        return NULL;
    }

    if (!ht) ht = salloc(sizeof(struct hasht), NULL);


    ht->num_buckets = num_buckets;
    //debug("allocating capacity %zu", capacity);
    ht->buckets = salloc(num_buckets * sizeof(struct hashtnode), NULL);
    return ht;
}

size_t Hasht_count(const struct hasht *ht){
    assert(ht);
    return ht->count;
}

bool Hasht_empty(const struct hasht *ht){
    return (Hasht_count(ht) == 0);
}

static inline void destroy_chains(struct hasht *ht){
    assert(ht);

    THROWS(ERROR_MISCONFIGURED, ht->dtor==NULL, "missing destructor");
    struct hashtnode *htn, *tmp;

    for (size_t i = 0; i < ht->num_buckets; ++i){
        htn = (ht->buckets+i)->next;
        while (htn){
            tmp = htn->next;
            ht->dtor(htn);
            htn = tmp;
        }
    }
}

void Hasht_clear(struct hasht *ht, bool free_containers){
    assert(ht);

    if (free_containers){
        destroy_chains(ht);
    }

    salloc(0, ht->buckets);
    ht->num_buckets = 0;
    ht->count = 0;

    /* reallocate a minimum number of buckets */
    allocate_hasht__(ht, 0);
}

void Hasht_destroy(struct hasht **hasht, bool free_containers){
    assert(hasht);
    assert(*hasht);

    struct hasht *ht = *hasht;

    if (free_containers){
        destroy_chains(ht);
    }

    salloc(0, ht->buckets);
    salloc(0, ht);
    *hasht = NULL;
}

struct hasht *Hasht_new(
        hashf hashfunc,
        key_getter keyfunc,
        hashtnode_destructor dtor,
        unsigned lf)
{
    THROWS(ERROR_MISCONFIGURED, keyfunc==NULL, "hasht key getter not provided");
    hashfunc = hashfunc ? hashfunc : HASHT_DEFAULT_HASHFUNC;
    lf = (lf>0) ? lf : HASHT_DEFAULT_LOAD_FACTOR;

    struct hasht *ht = allocate_hasht__(NULL, 0);
    assert(ht);  /* cannot exceed limit on first allocation */

    ht->count = 0;
    ht->lf = lf;
    ht->key = keyfunc;
    ht->hash = hashfunc;
    ht->dtor = dtor;

    return ht;
}

/*
 * Greatest mersenne prime under 64 bits. See
 * https://oeis.org/A000043 or https://www.mersenne.org/primes/  */
const uint64_t PRIME_SCRAMBLER = ((uint64_t)1 << 61) - 1;

static inline uint64_t hash2index(const struct hasht *ht, uint64_t hash){
    assert(ht);
    assert(ispow2(ht->num_buckets));
    return (hash % PRIME_SCRAMBLER) & (ht->num_buckets - 1);
}

static inline uint64_t node2index(
        const struct hasht *ht,
        const struct hashtnode *node)
{
    assert(ht);
    assert(node);
    assert(ht->key);
    assert(ht->hash);

    void *key_start = NULL;
    size_t key_len = 0;
    ht->key(node, &key_start, &key_len);

    assert(key_len > 0);
    assert(key_start != NULL);

    //debug("hashing key_start %p len=%zu", (void*)key_start, key_len);
    uint64_t hash = ht->hash(key_start, key_len);
    uint64_t idx = hash2index(ht, hash);

    assert(idx < ht->num_buckets);
    assert(ht->buckets != NULL);

    return idx;
}

// insert item without calling maybe_resize; to be called from inside
// the function doing the resizing (i.e. avoid triggering another
// resizing when doing a resizing)
void hasht_raw_set(struct hasht *ht, struct hashtnode *node){
    assert(ht);
    assert(node);

    uint64_t idx = node2index(ht, node);
    struct hashtnode *head = ht->buckets+idx;
    node->next = head->next;
    head->next = node;
}

// compare two keys as byte-strings
static bool equal_keys(const struct hasht *ht,
        const struct hashtnode *a, const struct hashtnode *b)
{
    assert(ht);
    assert(a);
    assert(b);
    assert(ht->key);

    char *ak=NULL, *bk=NULL;
    size_t aksz=0, bksz=0;

    ht->key(a, (void **)&ak, &aksz);
    ht->key(b, (void **)&bk, &bksz);

    assert(ak && bk);
    assert(aksz > 0 && bksz > 0);

    if (aksz != bksz) return false;
    if (strncmp(ak, bk, aksz)) return false;

    return true;
}

/*
 * Let clf be the current load factor just calculated.
 * Let slf be the load factor threshold specified at initialization time.
 * - grow (double) the number of buckets when clf > slf
 * - shrink (halve) the number of buckets when clf < (slf/8)
 *
 * (1) if a growing is to be done, resizing can fail if the max limit
 * is exceeded. In this case, NULL is returned. Otherwise, in case of
 * success (or no change), the same pointer that was passed in is returned
 * since the handle remains unchanged.
 */
struct hasht *maybe_resize(struct hasht *ht, bool isdel){
    assert(ht);
    unsigned load = (ht->count * 100) / ht->num_buckets;
    size_t newsize = ht->num_buckets;
    if (!isdel && load > ht->lf){           /* grow only when inserting */
        newsize <<= 1;
    } else if (isdel && load < (ht->lf>>3)){ /* shrink only when deleting */
        newsize >>= 1;
    }

    if (newsize == ht->num_buckets) /* no change; no resizing needed */
        return ht;

    if (isdel && newsize < HASHT_MIN_BUCKETS){
        /* do not shrink if when in deletion mode the capacity would
         * fall below minimum allowed */
        return ht;
    }

    //debug("load now: %u (count=%zu buckets=%zu) resizing needed (newsize=%zu) (ht->lf=%u)",
    //        load, ht->count, ht->num_buckets, newsize, ht->lf);

    struct hasht new;
    if (!allocate_hasht__(&new, newsize)) return NULL; /* (1) */
    new.hash = ht->hash;
    new.key = ht->key;
    new.count = 0;

    struct hashtnode *node, *tmp;
    /* rehash and move every element from the old table to the new one */
    for (size_t i = 0; i < ht->num_buckets; ++i){
        node = (ht->buckets+i)->next;
        while (node){
            tmp = node->next;
            hasht_raw_set(&new, node);
            node = tmp;
        }
    }

    salloc(0, ht->buckets);
    ht->buckets = new.buckets;
    ht->num_buckets = new.num_buckets;
    return ht;
}

struct hashtnode *Hasht_get_entry(
        struct hasht *ht,
        const struct hashtnode *node)
{
    assert(ht); assert(node);
    ht->cached = NULL;

    if (ht->count == 0) return NULL;

    uint64_t index = node2index(ht, node);
    struct hashtnode *i = (ht->buckets+index)->next;
    while (i){
        if (equal_keys(ht, i, node)){
            ht->cached = i; return i;
        }
        i = i->next;
    }

    return NULL;
}

bool Hasht_has_entry(const struct hasht *ht, const struct hashtnode *node)
{
    return (Hasht_get_entry((struct hasht*)ht, node) != NULL);
}

bool Hasht_remove_entry(
        struct hasht *ht,
        struct hashtnode *node,
        bool free_container)
{
    assert(ht); assert(node);
    if (ht->count == 0) return NULL;

    uint64_t index = node2index(ht, node);

    struct hashtnode *prev = (ht->buckets+index);
    struct hashtnode *i = prev->next;

    while (i){
        if (equal_keys(ht, i, node)) break;
        prev = i;
        i = i->next;
    }

    if (!i) return false;
    prev->next = i->next;

    if (free_container){
        THROWS(ERROR_MISCONFIGURED, ht->dtor==NULL, "missing destructor");
        ht->dtor(i);
    }

    ht->count--;
    maybe_resize(ht, true);

    return true;
}

// ensure no entry exists in the chain starting at head with the same key
// as node; head is the chain head (the first dummy node)
//
// (1) the new node is inserted at the start of the chain. Meaning subsequent
// insertions/deletions will find this first before any older duplicates
// (or other nodes in the list that are not duplicates but ended up in the
// same bucket due to collisions)
static inline bool unique(const struct hasht *ht,
        const struct hashtnode *head,
        const struct hashtnode *node)
{
    assert(head);
    assert(node);
    head = head->next;
    while (head){
        if (equal_keys(ht, head, node)) return false;
        head = head->next;
    }
    return true;
}

bool Hasht_insert_entry(struct hasht *ht, struct hashtnode *node,
        bool allow_duplicates)
{
    assert(ht); assert(node);

    uint64_t index = node2index(ht, node);
    struct hashtnode *head = ht->buckets+index;

    if (!allow_duplicates){
        if (!unique(ht, head, node)) return false;
    }

    node->next = head->next; /* (1) */
    head->next = node;

    ht->count++;
    if (!maybe_resize(ht, false)) return false;

    return true;
}

