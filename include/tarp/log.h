#ifndef TARP_LOG_H
#define TARP_LOG_H


#ifdef __cplusplus
extern "C" {
#endif


#include <tarp/common.h>

/*
 * Subset from man 3 syslog.
 * If a certain logging level is set, only messages with a log level value <= it
 * are reported when logged.
 */
enum logLevel {
    LOG_CRIT=0,
    LOG_ERR=1,
    LOG_WARNING=2,
    LOG_INFO=3,
    LOG_DEBUG=4
};

/*
 * return current log level */
int get_current_log_level(void);

/*
 * Set current log level to <level>, which must be one of the values in the
 * logLevel enumeration. */
int set_current_log_level(int level);

/*
 * see https://github.com/vcsaturninus/termite/termite.lua FMI.
 */
#define CSI          "\033["  /* \033=\e, but that's a nonstandard extension */
#define SGR_RESET    CSI"0m"

#define BLACK      30
#define RED        31
#define GREEN      32
#define YELLOW     33
#define BLUE       34
#define MAGENTA    35
#define CYAN       36
#define WHITE      37
#define BRED       91  /* bright blue */
#define BGREEN     92  /* bright green */

#define color(color, text)     CSI tkn2str(color)"m" text SGR_RESET 


void log_message(int priority, const char *fmt, ...);
#define crit(...)   log_message(LOG_INFO,    color(CYAN,     "[CRIT] ") __VA_ARGS__)
#define error(...)  log_message(LOG_ERR,     color(RED,      "[ERROR] ") __VA_ARGS__)
#define warn(...)   log_message(LOG_WARNING, color(BLUE,     "[WARN] ") __VA_ARGS__)
#define info(...)   log_message(LOG_INFO,    color(GREEN,    "[INFO] ") __VA_ARGS__)

#define debug__(fmt, ...) \
    log_message(LOG_DEBUG, \
        color(YELLOW,   "[DEBUG] ") "%s(),%s:%d | " \
        fmt "%s", __func__, __FILE__, __LINE__, __VA_ARGS__)

/* To silence warning about variadic macro expecting two params;
 * Notice the empty string passed as the second argument will be fed to the "%s"
 * format specifier placed after 'fmt' in the debug__ macro above; no matter
 * what 'fmt' is, a string format specifier is added at the end of it and this
 * empty string is fed to it */
#define debug(...) debug__( __VA_ARGS__, "")


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif  /* TARP_LOG_H */
