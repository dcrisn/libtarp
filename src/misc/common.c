#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <tarp/common.h>
#include <tarp/error.h>

#define DBGLOG_BUFFLEN 1024

void _dbglog_(int line, const char *file, const char *func, char *fmt, ...) {
    // save errno and restore at the end: just a precaution in case
    // some function HERE fails and changes errno
    int saved_errno = errno;

    // store msg here
    char buff[DBGLOG_BUFFLEN] = {'\0'};

    size_t left =
      DBGLOG_BUFFLEN - strlen(buff);  // at this stage, BUFFLEN-0, => BUFFLEN

    // start message with date and time
    time_t rawtime = time(NULL);
    struct tm *brokentime = localtime(&rawtime);
    strftime(buff, 30, "%a, %b %m %Y %H:%M:%S", brokentime);

    // append additional debug info
    snprintf(&(buff[strlen(buff)]),
             left,
             " -- [%s, %s(), L %d]:  Assertion failed because  ",
             file,
             func,
             line);
    left = DBGLOG_BUFFLEN - strlen(buff);

    // finally, append user message
    va_list args;
    va_start(args, fmt);
    vsnprintf(&(buff[strlen(buff) - 1]), left, fmt, args);
    va_end(args);

    // add trailing newline, if not already newline terminated
    if (buff[strlen(buff) - 1] != '\n') {
        buff[strlen(buff)] = '\n';
    }

    // print resulting msg
    fprintf(stderr, "%s", buff);
    fflush(stderr);

    // restore errno
    errno = saved_errno;
}

void *salloc(size_t size, void *ptr) {
    void *mem = NULL;

    if (size > 0) {
        if (ptr) mem = realloc(ptr, size);
        else mem = calloc(1, size);
    } else {
        free(ptr);
        return NULL;
    }

    THROW_ON(!mem, ERROR_BADALLOC);
    return mem;
}

// thread-safe error print.
void perr(const struct result res) {
    if (!res.e) {
        fprintf(stderr, "perr called with null result->e");
        return;
    }

    if (res.ok) {
        fprintf(stdout, "ok\n");
        return;
    }

    if (res.errnum == 0) {
        fprintf(stderr, "%s\n", res.e);
        return;
    }

    // thread-safe strerror.
    char buff[256];
    memset(buff, 0, sizeof(buff));

#ifdef _GNU_SOURCE
    // the GNU version of strerror_r rather than the XSI one
    const char *errstr = strerror_r(res.errnum, buff, sizeof(buff));
#else
    strerror_r(res.errnum, buff, sizeof(buff));
    const char *errstr = buff;
#endif

    fprintf(stderr, "%s: %s\n", res.e, errstr);
}
