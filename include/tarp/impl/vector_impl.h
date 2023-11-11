#ifndef TARP_VECTOR_IMPL_H
#define TARP_VECTOR_IMPL_H

#include <stdlib.h>
#include <stdint.h>
#include <tarp/error.h>
#include <tarp/common.h>

struct vector {
    size_t capacity;
    size_t occupied;
    size_t itemsz;

    // not a flexible array member so as not to invalidate the user's vector
    // handle on reallocation (only relocate 'bytes', not the whole vector)
    uint8_t *bytes;
};

typedef struct vector vector;

vector *allocate_vector__(struct vector *v, size_t capacity, size_t itemsz);
bool must_grow__(const struct vector *v);
bool must_shrink__(const struct vector *v);
struct vector *grow__(struct vector *v);
struct vector *shrink__(struct vector *v);

#define define_vector_new(SHORTNAME, TYPE)                          \
    vector *Vect_new##SHORTNAME(size_t capacity){                   \
        return allocate_vector__(NULL, capacity, sizeof(TYPE));     \
    }

#define define_vector_pushb(SHORTNAME, TYPE)                               \
    int Vect_pushb##SHORTNAME(vector *v, TYPE value){                      \
        THROW(ERROR_BADPOINTER, v == NULL);                                \
        THROW(ERROR_INVALIDVALUE, sizeof(TYPE) != v->itemsz);              \
                                                                           \
        if (must_grow__(v)){                                               \
            if (!grow__(v)) return ERROR_OUTOFBOUNDS;                      \
        }                                                                  \
                                                                           \
        TYPE *pos = get_pointer_at_position(v->bytes, TYPE, v->occupied);  \
        memcpy(pos, &value, sizeof(TYPE));                                 \
        v->occupied++;                                                     \
        return ERRORCODE_SUCCESS;                                          \
    }

#define define_vector_popb(SHORTNAME, TYPE)                                \
    TYPE Vect_popb##SHORTNAME(vector *v){                                  \
        THROW(ERROR_BADPOINTER, v == NULL);                                \
        THROW(ERROR_INVALIDVALUE, sizeof(TYPE) != v->itemsz);              \
        THROW(ERROR_EMPTY, v->occupied == 0);                              \
        if (must_shrink__(v)) shrink__(v);                                 \
        --v->occupied;                                                     \
        TYPE value;                                                        \
        TYPE *pos = get_pointer_at_position(v->bytes, TYPE, v->occupied);  \
        memcpy(&value, pos, sizeof(TYPE));                                 \
        return value;                                                      \
    }

#define define_vector_get(SHORTNAME, TYPE)                                 \
    TYPE Vect_get##SHORTNAME(const vector *v, size_t pos){                 \
        THROW(ERROR_BADPOINTER, v == NULL);                                \
        THROW(ERROR_OUTOFBOUNDS, pos >= v->occupied);                      \
        TYPE *ptr = get_pointer_at_position(v->bytes, TYPE, pos);          \
        return *ptr;                                                       \
    }

#define define_vector_set(SHORTNAME, TYPE)                                 \
    void Vect_set##SHORTNAME(vector *v, size_t pos, TYPE value){           \
        THROW(ERROR_BADPOINTER, v == NULL);                                \
        THROW(ERROR_OUTOFBOUNDS, pos >= v->occupied);                      \
        TYPE *ptr = get_pointer_at_position(v->bytes, TYPE, pos);          \
        *ptr = value;                                                      \
    }

#define define_vector_front(SHORTNAME, TYPE)                               \
        TYPE Vect_front##SHORTNAME(const vector *v){                       \
            THROW(ERROR_BADPOINTER, v == NULL);                            \
            THROW(ERROR_EMPTY, v->occupied == 0);                          \
            TYPE *ptr = get_pointer_at_position(v->bytes, TYPE, 0);        \
            return *ptr;                                                   \
        }

#define define_vector_back(SHORTNAME, TYPE)                                \
        TYPE Vect_back##SHORTNAME(const vector *v){                        \
            THROW(ERROR_BADPOINTER, v == NULL);                            \
            THROW(ERROR_EMPTY, v->occupied == 0);                          \
            TYPE *ptr = get_pointer_at_position(                           \
                    v->bytes, TYPE, (v->occupied-1));                      \
            return *ptr;                                                   \
        }

#define define_vector_begin(SHORTNAME, TYPE)                               \
        TYPE *Vect_begin##SHORTNAME(const vector *v){                      \
            THROW(ERROR_BADPOINTER, v == NULL);                            \
            TYPE *ptr = get_pointer_at_position(v->bytes, TYPE, 0);        \
            return ptr;                                                    \
        }

#define define_vector_end(SHORTNAME, TYPE)                                 \
        TYPE *Vect_end##SHORTNAME(const vector *v){                        \
            THROW(ERROR_BADPOINTER, v == NULL);                            \
            TYPE *ptr = get_pointer_at_position(                           \
                    v->bytes, TYPE, v->occupied);                          \
            return ptr;                                                    \
        }

#define define_vector_insert(SHORTNAME, TYPE)                              \
    int Vect_insert##SHORTNAME(vector *v, size_t pos, TYPE value){         \
        THROW(ERROR_BADPOINTER, v == NULL);                                \
        THROW(ERROR_OUTOFBOUNDS, pos > v->occupied);                       \
                                                                           \
        int rc = ERRORCODE_SUCCESS;                                        \
                                                                           \
        /* make room for 1; may trigger the vector to grow */              \
        if ((rc = Vect_pushb##SHORTNAME(v, value)) != 0) return rc;        \
        if (v->occupied == 1) return rc; /* was empty; inserted, done */   \
                                                                           \
        TYPE *posptr = get_pointer_at_position(v->bytes, TYPE, pos);       \
                                                                           \
        /* shift all elements from pos and to the right of it, one */      \
        /* position to the right; -1 because we pushed to the back above*/ \
        /* => decrement back for the calculation */                        \
        memmove(posptr+1, posptr, v->itemsz * (v->occupied-1-pos));        \
        Vect_set##SHORTNAME(v, pos, value);                                \
        return rc;                                                         \
    }

#define define_vector_remove(SHORTNAME, TYPE)                              \
    void Vect_remove##SHORTNAME(vector *v, size_t pos){                    \
        THROW(ERROR_BADPOINTER, v == NULL);                                \
        THROW(ERROR_OUTOFBOUNDS, pos >= v->occupied);                      \
        if (v->occupied==0) return;                                        \
                                                                           \
        TYPE *posptr = get_pointer_at_position(v->bytes, TYPE, pos);       \
                                                                           \
        /* -1 because we start at posptr+1 */                              \
        memmove(posptr, posptr+1, v->itemsz*(v->occupied-1-pos));          \
        Vect_popb##SHORTNAME(v); /* shrink by 1 */                         \
    }

#define define_vector_resize(SHORTNAME, TYPE)                              \
    void Vect_resize##SHORTNAME(vector *v, size_t count, TYPE fill){       \
        THROW(ERROR_BADPOINTER, v == NULL);                                \
        if (count == v->occupied) return;                                  \
        if (count > v->occupied){                                          \
            size_t stop = count - v->occupied;                             \
            for (size_t i = 0; i < stop; ++i){                             \
                Vect_pushb##SHORTNAME(v, fill);                            \
            }                                                              \
        }else {                                                            \
            size_t stop = v->occupied - count;                             \
            for (size_t i = 0; i < stop; ++i){                             \
                Vect_popb##SHORTNAME(v);                                   \
            }                                                              \
        }                                                                  \
    }

// like std::vector at but returns a pointer
#define define_vector_getptr(SHORTNAME, TYPE)                              \
    TYPE *Vect_getptr##SHORTNAME(const vector *v, size_t pos){             \
        THROW(ERROR_BADPOINTER, v == NULL);                                \
        THROW(ERROR_OUTOFBOUNDS, pos >= v->occupied);                      \
        return get_pointer_at_position(v->bytes, TYPE, pos);               \
    }

#endif
