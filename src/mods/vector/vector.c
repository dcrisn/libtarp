#include <tarp/common.h>
#include <tarp/log.h>
#include <tarp/vector.h>

/* 
 * Obviously, *at the very least* ensure
 * (SIZE_MAX - TARP_VECTOR_MAX_CAPACITY) >= sizeof(struct vector). */
#define TARP_VECTOR_MAX_MEM (SIZE_MAX>>2)
#define TARP_VECTOR_MIN_CAPACITY 16

size_t Vect_maxcap(size_t itemsz){
    return TARP_VECTOR_MAX_MEM / itemsz;
}

// if v = NULL, then alloc v as well, else only alloc bytes
vector *allocate_vector__(struct vector *v, size_t capacity, size_t itemsz)
{
    if (capacity < TARP_VECTOR_MIN_CAPACITY) capacity = TARP_VECTOR_MIN_CAPACITY;

    if (capacity > TARP_VECTOR_MAX_MEM){
        debug("vector with excessive capacity %zu requested", capacity);
        return NULL;
    }

    if (capacity > Vect_maxcap(itemsz)){
        debug("refusing to allocate vector (cap=%zu itemsz=%zu",
                capacity, itemsz);
        return NULL;
    }

    if (!v){
        v = salloc(sizeof(struct vector), NULL);
        v->occupied = 0;
    }

    v->capacity = capacity;
    v->itemsz = itemsz;
    v->bytes = realloc(v->bytes, capacity*itemsz);
    return v;
}

size_t Vect_count(const vector *v){
    assert(v);
    return v->occupied;
}

void Vect_destroy(vector **v){
    assert(v);
    salloc(0, (*v)->bytes);
    salloc(0, *v);
    *v = NULL;
}

/*
 * Growth and shrinkage policy:
 * - double the capacity when full
 * - reduce to half the capacity when below 1/4
 */
bool must_grow__(const struct vector *v){
    return (v->occupied == v->capacity);
}

bool must_shrink__(const struct vector *v){
    return (v->occupied < (v->capacity >> 2));
}

struct vector *grow__(struct vector *v){
    if (v->capacity > (Vect_maxcap(v->itemsz) >> 1)) return NULL;
    return allocate_vector__(v, v->capacity << 1, v->itemsz);
}

/*
 * NOTE
 * If this is to be changed to something more conservative e.g. reduce to 0.7
 * of the capacity when below half, then reduce TARP_VECTOR_MAX_MEM as
 * appropriate to ensure no overflow when calculating the percentage in
 * v->capacity. */
struct vector *shrink__(struct vector *v){
    return allocate_vector__(v, v->capacity >> 1, v->itemsz);
}

void Vect_clear(vector *v){
    assert(v); 
    v->occupied = 0;
}

bool Vect_isempty(const vector *v){
    return (v->occupied == 0);
}
