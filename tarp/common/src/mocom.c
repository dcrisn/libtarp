#include <time.h>
#include <errno.h>
#include <stdarg.h>

#include <tarp/mocom.h>

/*
 * This function is what realloc should be, it behaves like
 * calloc, free or realloc depending on passed parameters.
 *
 * Practically, realloc behaves in the same way everywhere
 * but it is not officially mandated to do so by THE STANDARD:
 * http://www.openstd.org/jtc1/sc22/wg14/www/docs/n1256.pdf#page=326
 *
 * falloc_counter must be a global variable. It's incremented by memory allocation
 * calls and decremented by calls to free().
 * To ensure 0 memory leaks, the calls must be perfectly ballanced i.e. mem_alloc_counter
 * must be 0 at program exit.
 *
 * calloc is used rather than malloc so that the memory is always zeroed out in
 * order to avoid gibberish.
 */
int falloc_counter = 0; /* used by falloc() */
void *falloc(void *ptr, size_t size){
    void *res = NULL;

    // act as free - decrement counter
    if(ptr && !size){
          falloc_counter--;
          free(ptr);
          res = NULL;
    }
    // act as calloc
    else if (!ptr && size){
        falloc_counter++;
        res = calloc(size, sizeof(char));
        assert(res);
    }
    // if ptr !NULL and size is specified, then it's a realloc call
    else if(ptr && size){
        res = realloc(ptr, size);
        assert(res);
    }
    return res;
}

#define DBGLOG_BUFFLEN 1024
void _dbglog_(int line, const char *file, const char *func, char *fmt, ...){
    // save errno and restore at the end: just a precaution in case
    // some function HERE fails and changes errno
    int saved_errno = errno;

    // store msg here
    char buff[DBGLOG_BUFFLEN] = {'\0'};

    size_t left = DBGLOG_BUFFLEN - strlen(buff); // at this stage, BUFFLEN-0, => BUFFLEN

    // start message with date and time
    time_t rawtime = time(NULL);
    struct tm *brokentime = localtime(&rawtime);
    strftime(buff, 30, "%a, %b %m %Y %H:%M:%S", brokentime);

    // append additional debug info
    snprintf(&(buff[strlen(buff)]),  left, " -- [%s, %s(), L %d]:  Assertion failed because  ", file, func, line);
    left = DBGLOG_BUFFLEN - strlen(buff);

    // finally, append user message
    va_list args;
    va_start(args, fmt);
    vsnprintf(&(buff[strlen(buff)-1]), left, fmt, args);
    va_end(args);

    // add trailing newline, if not already newline terminated
    if (buff[strlen(buff)-1] != '\n'){
        buff[strlen(buff)] = '\n';
    }

    // print resulting msg
    fprintf(stderr, "%s", buff);
    fflush(stderr);

    // restore errno
    errno = saved_errno;
}


