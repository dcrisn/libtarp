#ifndef TARP_ERROR_H
#define TARP_ERROR_H

#include <tarp/log.h>

#ifdef __cplusplus
extern "C" {
#endif

enum Exception{
    BAD_ALLOC=0,
    EXCEPTION_LAST__
};

const char *get_exception_string(unsigned int code);

void throw__(
        enum Exception code,
        const char *file,
        const char *func,
        int line);


#define THROW(exception, cond) \
    do { if (cond) throw__(exception, __FILE__, __func__, __LINE__); }while(0)

#define ASSUME(cond, ...) do{ if (!cond) warn(__VA_ARGS__); } while(0)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
