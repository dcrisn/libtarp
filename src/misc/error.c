#include <tarp/error.h>
#include <tarp/log.h>

static const char *error_codes[] = {
    [ERRORCODE_SUCCESS]      = "Success",
    [ERROR_BADALLOC]         = "Memory allocation failure",
    [ERROR_RUNTIMEERROR]     = "Fatal runtime error",
    [ERROR_NOSPACE]          = "Insufficient buffer size",
    [ERROR_OUTOFBOUNDS]      = "Value outside acceptable bounds",
    [ERROR_BADPOINTER]       = "NULL or otherwise invalid pointer provided",
    [ERROR_INVALIDVALUE]     = "Unacceptable value provided",
    [ERROR_EMPTY]            = "Cannot read from or access empty resource"
};

const char *tarp_strerror(unsigned int code){
    assert(code < ERRORCODE_LAST__);
    return error_codes[code];
}

void throw__(
        enum ErrorCode code,
        const char *file,
        const char *func,
        int line,
        const char *condition,
        const char *extra)
{
    const char *errmsg = tarp_strerror(code);
#ifdef DEBUG_BUILD
    fprintf(stderr, "%s [%s:%d -> %s(), condition: '%s'] %s %s\n",
            errmsg, file, line, func, condition,
            extra ? "~" : "", extra ? extra : "");
#else
    UNUSED(file); UNUSED(line);
    fprintf(stderr, "%s [%s()] %s %s\n",
            errmsg, func,
            extra ? " ~ " : "",
            extra ? extra : "");
#endif
    exit(EXIT_FAILURE);
}

