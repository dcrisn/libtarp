#include <unistd.h>

#include <iostream>
#include <utility>
#include <chrono>
#include <memory>

#include <tarp/event.hxx>
#include <tarp/log.h>

using namespace std;
using namespace std::chrono;
using namespace tarp;

/*
 * A basic example of using part of the C++ events api.
 * NOTE error checking and such is ommitted for simplicity.
 *
 * 3 callbacks are created:
 * - 'reader': a fd-event callback that's called when the read-end of a
 *   pipe is readable; stops itself after 'n' invocations.
 * - 'writer': a timer-based callback that writes to the pipe on an interval.
 *   Also stops itself after some 'n' invocations.
 *
 * - a 'explicit' callback, explicitly instantiated and accessible to the
 *   user. Also stops itself after some 'n' invocations; it also doubles its
 *   interval duration duration with each invocation.
 */

int main(int argc, char *argv[]){
    UNUSED(argc); UNUSED(argv);

    auto evp = make_event_pump();
    assert(evp);
    auto x = evp->make_explicit_timer_callback(0s);
    x->set_func([Cb=x, interval=100ms, n=0]() mutable{
                info("explicit callback called (n=%d, interval=%d ms)",n, interval);

                if (n++ == 6){
                    return false; // signals destruction
                }

                interval *= 2;
                Cb->set_interval(interval);
                return true;
            });

#if 1
    // fd-event reader and timer-based writer callbacks managed by EventPump
    int fds[2];
    pipe(fds);

    auto writer = [fd=fds[1], n=0](void) mutable{
        debug("Timed writer callback called, n=%d", n);
        uint8_t buff[8] = "21en121";
        write(fd, buff, 8);

        if (++n == 20){
            info("writer canceling");
            return false; // run n times
        }

        return true;
    };


    auto reader = [n=0](int fd) mutable{
        debug("reader called, n=%d", n);
        uint8_t buff[8];
        read(fd, buff, 8);

        if (++n == 15){
            info("reader canceling");
            return false; // run n times
        }

        return true;
    };

    evp->set_timer_callback(500ms, writer);
    evp->set_fd_event_callback(fds[0], FD_EVENT_READABLE, reader);

    UNUSED(reader);
    UNUSED(writer);
#endif
    x->activate();

    /*
     * If we remove the reference to the TimerEventCallback that anchors
     * it to this scope, then it will be destroyed when the lambda
     * returns false (assuming there are no other references to the
     * TimerEventCallback object *or* the lambda anywhere else to keep them
     * alive.
     */
    x = nullptr;

    evp->run();
}



