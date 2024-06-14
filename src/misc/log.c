#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include <tarp/log.h>


#define DEFAULT_LOG_LEVEL 4


static int current_log_level = DEFAULT_LOG_LEVEL;

int get_current_log_level(void){
    return current_log_level;
}

void log_message(int priority, const char *fmt, ...){
    if (priority > get_current_log_level()) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

int set_current_log_level(int level){
    assert(level >= LOG_CRIT && level <= LOG_DEBUG);
    switch(level){
    case LOG_DEBUG:
    case LOG_INFO:
    case LOG_WARNING:
    case LOG_ERR:
    case LOG_CRIT:
        current_log_level = level;
        return 0;
    default:
        return 1; /* error */
    }
}



