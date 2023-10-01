#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>  /* waitpid and macros */
#include <string.h>    /* memset */
#include <stdbool.h>
#include <time.h>      /* clock_gettime, clock_nanosleep */

#include <tarp/demonic.h>
#include <tarp/error.h>

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

/*
 * Populate the environment with the variables in the vars array.
 *
 * --> vars
 *     Must be non-NULL and non-empty and may contain an arbitrary
 *     number of strings. The last string in the array MUST be NULL.
 *     Each string must be an `=`-separated key-value pair. The equals sign
 *     ('=') separator must only appear ONCE in the string.
 *
 * <-- return
 *     The return value is either 0 if successful, a negative error number
 *     which is internal to this function, or a positive error number which 
 *     is an errno value set by e.g. setenv().
 *
 *     r < 0:
 *      * -1 == null array or empty array
 *      * -2 == key without value; parse failure
 *     r > 0:
 *      * see man setenv
 */
int populate_environment(const char * const vars[]){
    /* NULL argument or empty vars array */
    if (!vars || ! *vars){
        return -1;
    }

    const char *delim = "=";
    enum {ENV_VAR_LEN=2048};  /* arbitrary, should be plenty */
    
    char buff[ENV_VAR_LEN+1];
    for (const char *const *s = vars; s && *s; ++s){
        char *strtok_var = NULL;
        strncpy(buff, *s, ENV_VAR_LEN);
        char *key = strtok_r(buff, delim, &strtok_var);
        char *val = strtok_r(NULL, delim, &strtok_var);

        if (! (key && val)){
            return -2;  /* parse failure */
        }
        
        errno = 0;
        if (setenv(key, val, 1 /* overwrite */) == -1){
            return errno;
        }
    }
    return 0;
}

/*
 * Execute a subprocess according to the command specification in cmdspec.
 * 
 * The subprocess can optionally be passed an arbitrary number of environment
 * variables and will write or read from the specified descriptors.
 *
 * --> cmdspec
 *     A command specification that indicates the program to execute and the 
 *     options to be passed to it. The first string must be the path to the
 *     program to execute. The last string must be NULL.
 *
 * --> envspec
 *     '='-separated key-value strings to be set in the environment. See 
 *     populate_environment() above. If NULL, the subprocess will simply
 *     inherit the environment of the parent. If not NULL, the subprocess
 *     will inherit the environment of the parent but the environment is
 *     first modified such that the specified variables are set. If a
 *     variable with the same name already exists, it's overwritten.
 *
 * --> mstimeout
 *     A timeout value in milliseconds. If the subprocess hasn't terminated
 *     within mstimeout, it will get killed via SIGKILL. -1 means infinite 
 *     timeout. Note this function willl not return until the subprocess has
 *     exited one way or another.
 *
 * --> instream
 *     The file descriptor to set as the stdin of the subprocess.
 *     Pass -1 to ignore this option.
 *
 * --> outstream
 *     The file descriptor to set as the stdout of the subprocess.
 *     Pass -1 to ignore this option.
 *
 * --> errstream
 *     The file descriptor to set as the stderr of the subprocess. 
 *     Pass -1 to ignore this option.
 *
 * --> exit_code
 *     If not NULL, the exit code of the subprocess will be written to it.
 *     Note this is ONLY set if the chilld has terminated normally (that is
 *     not to say without error -- see man waitpid, WIFEXITED).
 *
 * <-- return
 *     The return value is either 0 (success), negative error number that is 
 *     internal and specific to this function as described below, or positive
 *     error number that is a value of errno set or returned by one of the 
 *     system call wrappers.
 *     
 *     r < 0:
 *      * -1 = error when setting environment variables
 *      * -2 = child did not exit normally (see man waitpid, WIFEXITED)
 *      * -3 = timed out, had to kill subprocess
 *
 *     r > 0:
 *      * an errno value returned or set by any of the functions called:
 *        waitpid, kill, exec, fork, clock_gettime, clock_nanosleep etc.
 */
int subprocess(char *const cmdspec[], const char * const envspec[], int mstimeout, int instream, int outstream, int errstream, int *exit_code){
    bool ischild = false;
    int rc = 0;
    errno  = 0;

    pid_t pid = fork();
    switch(pid){
        case -1:
            return errno;
            break;
        case 0:
            ischild = true; 
        default:
            break;
    }

    /* child subprocess */
    if (ischild){

        /* fix up environment */
        if (envspec){
            rc = populate_environment(envspec);
            if (rc > 0) return rc;  /* errno value */
            if (rc < 0) return -1;
        }
        
        /* Duplicate subprocess descriptors for standard streams */
        if ( (instream >= 0 && dup2(STDIN_FILENO, instream) == -1) ||
                (outstream >= 0 && dup2(STDOUT_FILENO, outstream) == -1) || 
                (errstream >= 0 && dup2(STDERR_FILENO, errstream) == -1) )
        {
            return errno;
        }

        /* forked; now exec */
        if (execv(*cmdspec, cmdspec) == -1){  /* only returns on error */
            return errno;
        }
    }

    /* parent (i.e. calling) process */
    else {
        struct timespec timespec; 
        memset(&timespec, 0, sizeof(struct timespec));
        if (clock_gettime(CLOCK_MONOTONIC, &timespec) == -1){
            return errno; 
        }

        /* todo -- replace with calls to functions in timeutils.h */
        uint64_t s  = timespec.tv_sec  + (mstimeout/1000);
        uint64_t ns = timespec.tv_nsec + (mstimeout % 1000);
        timespec.tv_sec = s + ns / 1000000000; /* 1e9 */
        timespec.tv_nsec = ns % 1000000000;

        if (mstimeout > 0){
            while ( (rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &timespec, NULL)) ){
                if (rc != EINTR){
                    return rc; /* errno */
                }
            }
        }
        
        int exit_status = 0;
        pid_t exitted_child = 0;
        if ((exitted_child = waitpid(pid, &exit_status, mstimeout > 0 ? WNOHANG : 0)) == -1){
            return errno; 
        } else if (exitted_child > 0){
            if (!WIFEXITED(exit_status)){
                return -2; /* child did not exit normally */
            }
            int child_exit_code = WEXITSTATUS(exit_status);
            if (exit_code){
                *exit_code = child_exit_code;
            }
            return 0;
        } else { /* child not exitted yet */
            if (kill(pid, SIGKILL) == -1){
                return errno; /* failed to kill */
            }
            return -3; /* timed out, had to kill */
        }
    }
    
    return 0;
}


