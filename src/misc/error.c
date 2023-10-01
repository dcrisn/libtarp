#include <tarp/error.h>

static const char *exception_codes[] = {
    [BAD_ALLOC] = "Memory allocation failure",
};

const char *get_exception_string(unsigned int code){
    assert(code < EXCEPTION_LAST__);
    return exception_codes[code];
}

void throw__(
        enum Exception code,
        const char *file,
        const char *func,
        int line)
{
    const char *errmsg = get_exception_string(code);
#ifdef DEBUG_BUILD
    fprintf(stderr, "%s [%s:%d -> %s()]\n", errmsg, file, line, func);
#else
    UNUSED(file); UNUSED(func); UNUSED(line);
    fprintf(stderr, "%s\n", errmsg);
#endif
    exit(EXIT_FAILURE);
}

