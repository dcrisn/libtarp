#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <tarp/container.h>
#include <tarp/types.h>
#include <unistd.h>
#include <stdbool.h>

#include <tarp/process.h>
#include <tarp/common.h>
#include <tarp/log.h>
#include <tarp/event.h>
#include <tarp/staq.h>
#include <tarp/cohort.h>


/*
 * basic tests for for the C {sync,async}_exec API.
 *
 * Note the tests here are not comprehensive. They don't test everything
 * and won't catch all possible bugs. Rather the point of them is to have
 * a base to build on and expand if bugs are found and to run the available
 * sanitizers on the test binary.
 */

#define SCRIPTS_PATH "./scripts"
#define SHELL_SCRIPT_TO_USE SCRIPTS_PATH "/" "exec_test.sh"
#define MAX_STRING_SIZE 2048

struct string_qwrap{
    struct staqnode link;
    const char *s;
};

void staq_node_destructor(struct staqnode *node){
    struct string_qwrap *ud = get_container(node, struct string_qwrap, link);
    salloc(0, (void *)ud->s);
    salloc(0, ud);
}

static struct string_pair environment_vars[] = {
    {"one", "first"},
    {"two", "second"},
    {"three", "third"},
    {"four", "fourth"},
    {NULL, NULL}
};

static const char *const exec_command[] = {SHELL_SCRIPT_TO_USE, NULL};

static inline void exec_output_cb(enum stdStream stream,
        int fd, unsigned events, void *priv)
{
    UNUSED(priv);
    UNUSED(events);
    UNUSED(fd);
    UNUSED(stream);
    UNUSED(priv);

    struct ptr_4tup *tup = priv;
    struct staq *q = tup->third;
    if (stream == STDOUT || stream == STDERR){
        //debug("exec output cb called, events=%d", events);
        void *s = salloc(MAX_STRING_SIZE, NULL);
        struct string_qwrap *sw = salloc(sizeof(struct string_qwrap), NULL);
        sw->s = s;

        int bytes_read = read(fd, s, MAX_STRING_SIZE-1);
        UNUSED(bytes_read);

        Staq_enq(q, sw, link);
    }

    return;
}

static inline void exec_completion_cb(pid_t pid, int status, void *priv){
    debug("process pid=%d has exited with status %d", pid, status);

    assert(priv);
    struct int_pair *pair = ((struct ptr_4tup *)priv)->fourth;
    pair->second = (status == pair->first) ?  TEST_PASS : TEST_FAIL;

    // first initially contains the expected status
    // then we set the actual status; it's therefore an in-out param
    pair->first = status;
}

// wrapper so exec is called after the start of the event pump
void delayed_exec(struct timer_event *tev, void *priv){
    UNUSED(tev);

    assert(priv);
    struct ptr_4tup *tup= priv;
    int timeout = *(int *)tup->second;

    int rc = async_exec(
            tup->first,
            exec_command,
            environment_vars,
            true,
            STREAM_ACTION_DEVNULL,
            STREAM_ACTION_JOIN,
            STREAM_ACTION_JOIN,
            timeout,
            NULL,
            exec_output_cb,
            exec_completion_cb,
            tup
            );
    if (rc == -1){
        debug("async_exec failure");
        exit(EXIT_FAILURE);
    }
}

enum testStatus test_async_exec(
        struct evp_handle *evp,
        int exec_timeout,
        int expected_exit_status,
        int evp_run_timeout
        )
{
    assert(evp);
    struct staq q = STAQ_INITIALIZER(staq_node_destructor);

    struct int_pair ipair; /* status, expected */
    ipair.first = expected_exit_status;
    struct ptr_4tup pointers; /* evp_handle, exec timeout, queue, ipair */

    struct timer_event tev;
    Evp_init_timer_secs(&tev, 1, delayed_exec, &pointers);


    Evp_register_timer(evp, &tev);

    // pass data to the callbacks
    pointers.first = evp;
    pointers.second = &exec_timeout;
    pointers.third = &q;
    pointers.fourth= &ipair;

    Evp_run(evp, evp_run_timeout);

    debug("process exit code: %d", ipair.first);

    //debug("Read:");
    struct string_qwrap *i;
    unsigned num = 0;
    Staq_foreach(&q, i, struct string_qwrap, link){
        ////printf("READ: <%s>", i->s);
        // stop at newline (last char before NUL, added by the script prints)
        //debug("comparing '%s' with '%s'", i->s, environment_vars[num].second);
        if (!matchn(i->s, environment_vars[num].second,
                    strlen(environment_vars[num].second)))
        {
            ipair.second = TEST_FAIL; break;
        }
        ++num;
    }

    Staq_clear(&q, true);

    return ipair.second; // test status
}

enum testStatus test_sync_exec(int exec_timeout, int expected_exit_status)
{
    struct staq q = STAQ_INITIALIZER(staq_node_destructor);

    struct int_pair ipair; /* status, expected */
    ipair.first = expected_exit_status;
    struct ptr_4tup pointers; /* NULL (unused), NULL (unused), queue, NULL (unused) */

    // pass data to the callbacks
    pointers.third = &q;
    pointers.fourth= &ipair;

    int status = PROC_NOSTATUS;
    int rc = sync_exec(exec_command, environment_vars, true, STREAM_ACTION_DEVNULL,
            STREAM_ACTION_JOIN, STREAM_ACTION_JOIN,
            exec_timeout, &status, NULL,
            exec_output_cb, &pointers);

    if (rc == -1){
        error("cal to sync_exec failed");
        exit(EXIT_FAILURE);
    }

    debug("process exit code: %d", status);

    //debug("Read:");
    struct string_qwrap *i;
    unsigned num = 0;
    Staq_foreach(&q, i, struct string_qwrap, link){
        //printf("READ: <%s>", i->s);
        // stop at newline (last char before NUL, added by the script prints)
        //debug("comparing '%s' with '%s'", i->s, environment_vars[num].second);
        if (!matchn(i->s, environment_vars[num].second,
                    strlen(environment_vars[num].second)))
        {
            ipair.second = TEST_FAIL; break;
        }
        ++num;
    }

    Staq_clear(&q, true);

    return status == expected_exit_status ? TEST_PASS : TEST_FAIL;
}


int main(int argc, char **argv){
    UNUSED(argc);
    UNUSED(argv);

    prepare_test_variables();
    struct evp_handle *handle = Evp_new();

   /*==========================
     * Test 1:
     * - run so that the script exits normally: timeout > the amount of time
     *   the script sleeps for.
     * - compare the prints from the script (environment variables printed to
     *   stdout and stderr) which are stored as strings in the queue 'q',
     *   with the expected outputs.
     * - compare exit code of script with expected output.
     */
    printf("Validating async_exec outputs, no timeout\n");
    run(test_async_exec, TEST_PASS, handle,
            3000, /* 3s */
            0,
            4    /* 4s */
            );

    /*
     * Test 2:
     *  - like Test 1, but the subprocess is killed on timeout.
     */
    printf("Validating async_exec outputs, with timeout\n");
    run(test_async_exec, TEST_PASS, handle,
            1000, /* 1s */
            PROC_KILLED,
            4    /* 4s */
            );

    /*
     * Test 3:
     *  - like Test 1, but the execution is carried out synchronoulsy --
     *  not event pump system involved.
     */
    printf("Validating sync_exec outputs, no timeout\n");
    run(test_sync_exec, TEST_PASS, 3000, 0 /* 3s */);

    /*
     * Test 4:
     *  - like Test 2, but the execution is carried out synchronoulsy --
     *  no event pump involved.
     */
    printf("Validating sync_exec outputs, with timeout\n");
    run(test_sync_exec, TEST_PASS, 1000, PROC_KILLED /* 1s */);

    Evp_destroy(&handle);
    report_test_summary();
}

