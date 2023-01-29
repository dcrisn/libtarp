#ifndef __DEMONIC__
#define __DEMONIC__

/* 
 * Daemonize a process
 *
 * <-- return
 *     -1 on error; 0 on success 
 */
int daemonize(void);

int subprocess(char *const cmdspec[], const char * const envspec[], int mstimeout, int instream, int outstream, int errstream, int *exit_code);
int populate_environment(const char * const vars[]);

#endif
