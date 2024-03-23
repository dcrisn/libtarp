#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <tarp/common.h>

namespace tarp {

class ThreadEntity {
public:
    ThreadEntity(void);
    virtual ~ThreadEntity(void);

    bool run();
    void pause(void);
    void stop(void);

    bool is_paused(void) const;
    bool is_stopped(void) const;
    bool is_running(void) const;

protected:
    enum threadState { INITIALIZED, RUNNING, PAUSED, STOPPED };

    DISALLOW_COPY_AND_MOVE(ThreadEntity);
    void set_state(enum threadState state);

    /*
     * Hooks derived classes can override -- NOPs by default;
     * NOTE: all these hooks run in a mutex-protected context
     * inside the main loop, so they are thread-safe (for the
     * purposes of changing the state of this class).
     *
     * (1) called *once* after the thread is spawned,
     * before the main loop starts.
     *
     * (2) called when run() is invoked on a paused thread entity.
     * This can be used for reinitialization when waking from a pause.
     *
     * (3) called for every pass around the main loop. This is the
     * main thing the thread entity does. There is no sleep to speak
     * of in the main loop so the user should sleep in this function
     * as approprirate if any delays are needed.
     *
     * (4) called after stop() is called expicitly or in the destructor
     * of the thread entity.
     */
    virtual void initialize(void);     /* (1) */
    virtual void prepare_resume(void); /* (2) */
    virtual void do_task(void) = 0;    /* (3) */
    virtual void cleanup(void);        /* 4 */

private:
    void spawn(void);
    void loop(void);
    void wake(std::unique_lock<std::mutex> &&guard);

    mutable std::mutex m_mtx;
    std::thread m_thread;
    std::condition_variable m_flag;
    enum threadState m_state = STOPPED;
};



}  // namespace tarp
