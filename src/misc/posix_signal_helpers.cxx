#include <tarp/posix_signal_helpers.hxx>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tarp {

#define SET_SIGNAL_HANDLER(signal, sig_action_struct, exit_on_fail) \
    if (sigaction(signal, sig_action_struct, NULL) == -1) {         \
        fprintf(stderr,                                             \
                "Failed to set %s signal handler: '%s'",            \
                strsignal(signal),                                  \
                strerror(errno));                                   \
        if (exit_on_fail) exit(EXIT_FAILURE);                       \
    }

void set_up_signals(std::initializer_list<int> signals,
                           sighandler_t sighandler,
                           bool exit_on_fail,
                           bool verbose) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sighandler;
    sigemptyset(&sa.sa_mask);

    for (auto &sig : signals) {
        if (verbose)
            fprintf(stderr, "setting up signal for: %s\n", strsignal(sig));
        SET_SIGNAL_HANDLER(sig, &sa, exit_on_fail);
    }
}

}  // namespace tarp
