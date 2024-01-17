#include <map>
#include <vector>
#include <memory>
#include <string>
#include <initializer_list>

#include <tarp/process.h>
#include <tarp/log.h>
#include <tarp/event.hxx>
#include <tarp/cohort.h>

using namespace std;
using namespace tarp;

/*
 * basic tests for for the Cxx Process api.
 *
 * Note the tests here are not comprehensive. They don't test everything
 * and won't catch all possible bugs. Rather the point of them is to have
 * a base to build on and expand if bugs are found and to run the available
 * sanitizers on the test binary.
 */

#define SCRIPTS_PATH "./scripts"
#define SHELL_SCRIPT_TO_USE SCRIPTS_PATH "/" "exec_test.sh"
#define MAX_STRING_SIZE 2048

static map<string, string> environment_vars = {
    {"one", "first"},
    {"two", "second"},
    {"three", "third"},
    {"four", "fourth"},
};

static initializer_list<string> exec_command{SHELL_SCRIPT_TO_USE};

enum testStatus test_process_exec(
        shared_ptr<EventPump> evp,
        int exec_timeout,
        int expected_exit_status,
        int evp_run_timeout,
        bool asynchronous
        )
{
    assert(evp);
    auto p = evp->make_process(
            exec_command,
            exec_timeout,
            STREAM_ACTION_PASS,
            STREAM_ACTION_PIPE,
            STREAM_ACTION_PIPE,
            nullptr,
            nullptr);

    p->set_environment(environment_vars);
    p->run(asynchronous);
    if (asynchronous) evp->run(evp_run_timeout);

    auto status = p->get_exit_code();
    if (!status.has_value()){
        debug("process with pid %d without exit status", p->get_pid());
        return TEST_FAIL;
    }

    debug("process pid=%d exited with code %d", p->get_pid(), status.value());
    if (status.value() != expected_exit_status)
        return TEST_FAIL;

    auto outbuff = p->outbuff();
    auto errbuff = p->errbuff();

    string out(outbuff.begin(), outbuff.end());
    string err(errbuff.begin(), errbuff.end());

    // first two vars are stdout (see environment_vars at the top of the file)
    if (out != "first\nsecond\n") return TEST_FAIL;
    if (err != "third\nfourth\n") return TEST_FAIL;

    return TEST_PASS;
}

int main(int argc, char **argv){
    UNUSED(argc);
    UNUSED(argv);

    prepare_test_variables();
    auto evp = make_event_pump();

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
    passed = run(test_process_exec, TEST_PASS, evp,
            3000, /* 3s */
            0,
            4,    /* 4s */
            true
            );
    update_test_counter(passed, test_async_exec);

    /*
     * Test 2:
     *  - like Test 1, but the subprocess is killed on timeout.
     */
    printf("Validating async_exec outputs, with timeout\n");
    passed = run(test_process_exec, TEST_PASS, evp,
            1000, /* 1s */
            PROC_KILLED,
            4,    /* 4s */
            true
            );
    update_test_counter(passed, test_async_exec);

    /*
     * Test 3:
     *  - like Test 1, but the execution is carried out synchronoulsy --
     *  no event pump system involved.
     */
    printf("Validating sync_exec outputs, no timeout\n");
    passed = run(test_process_exec, TEST_PASS, evp, 3000 /*3s */,
            0, 0, false);
    update_test_counter(passed, test_async_exec);

    /*
     * Test 4:
     *  - like Test 2, but the execution is carried out synchronoulsy --
     *  no event pump involved.
     */
    printf("Validating sync_exec outputs, with timeout\n");
    passed = run(test_process_exec, TEST_PASS, evp, 1000 /*1s */,
            PROC_KILLED, 0, false);

    update_test_counter(passed, test_async_exec);

    report_test_summary();
}

