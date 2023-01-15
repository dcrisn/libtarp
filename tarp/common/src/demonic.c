#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include <tarp/demonic.h>
#include <tarp/common.h>

/*
 * Turn the calling process into a daemon.
 *
 * <-- return
 *     -1 on failure (failed to daemonize) and errno is set 
 *     as appropriate. If successful, only the child reaches 
 *     the end of the function and gets 0 as the return value.
 */
int daemonize(void){
    ssize_t fd;

    switch(fork()){                     /* fork to background */
        case -1: return -1;
        case 0: break;                  /* child gets 0 and should break out of the switch and continue on */
        default: exit(EXIT_SUCCESS);    /* parent gets (positive) PID of child) and should exit */
    }
    /* child carries on ... */

    /* Become leader of a new session */
    if (setsid() == -1) return -1;
    
    /* fork AGAIN to ensure the program can never reacquire
     * a controlling terminal, per  SysV conventions */
    switch(fork()){
        case -1: return -1;
        case 0 : break;                 /* child (grandchild of original process) should continue */
        default: exit(EXIT_SUCCESS);
    }
    
    /* clear umask; allow all permissions */
    umask(0);
    
    /* cd to /, so as not to prevent shutdown by keeping the fs from being unmounted */
    ASSUME((chdir("/") == 0), "FAILED to change directory to '/'");

    /* close stdin, stdout, stderr: use /dev/null instead so that io operations don't fail; 
     * /dev/null can always be written to, discarding everything, and can always be read from, 
     * always giving EOF. */
    ASSUME((close(STDIN_FILENO) == 0), "Failed to close stdin: '%s'", strerror(errno));
    /* point stdin to /dev/null: Fd should be 0 as Linux reuses the smallest 
     * fd number available (and stdin is 0) */ 
    fd = open("/dev/null", O_RDWR);
    ASSUME((fd == STDIN_FILENO), "Failed to point stdin to /dev/null: '%s'", strerror(errno));

    /* point the file descriptor in $2 to the same open file DESCRIPTION that $1 points to; i.e.
     * point stdout and stderr to /dev/null as well */
    ASSUME( (dup2(STDIN_FILENO,  STDOUT_FILENO) == STDOUT_FILENO), 
            "Failed to point stdout to /dev/null: '%s'", strerror(errno));
    ASSUME( (dup2(STDIN_FILENO,  STDERR_FILENO) == STDERR_FILENO), 
            "Failed to point stderr to /dev/null: '%s'", strerror(errno));

    return 0; /* only child reaches here */
}


