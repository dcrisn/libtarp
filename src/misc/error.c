#include <tarp/error.h>
#include <tarp/log.h>

static const char *exception_codes[] = {
    [EXCEPTION_BAD_ALLOC] = "Memory allocation failure",
    [EXCEPTION_RUNTIME_ERROR] = "Fatal runtime error"
};

static const char *error_codes[] = {
    [ERROR_CODE_FIRST__] = "Success",
    [ERROR_NOSPACE] = "Insufficient buffer size",
    [ERROR_OUTOFBOUNDS] = "Value outside acceptable bounds"
};

const char *tarp_strexcept(unsigned int code){
    assert(code < EXCEPTION_LAST__);
    return exception_codes[code];
}

const char *tarp_strerror(unsigned int code){
    assert(code < ERROR_CODE_LAST__);
    return error_codes[code];
}

void throw__(
        enum Exception code,
        const char *file,
        const char *func,
        int line)
{
    const char *errmsg = tarp_strexcept(code);
#ifdef DEBUG_BUILD
    fprintf(stderr, "%s [%s:%d -> %s()]\n",
            errmsg, file, line, func);
#else
    UNUSED(file); UNUSED(func); UNUSED(line);
    fprintf(stderr, "%s\n", errmsg);
#endif
    exit(EXIT_FAILURE);
}

