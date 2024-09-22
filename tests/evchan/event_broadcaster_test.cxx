#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <thread>
#include <map>
#include <utility>

#include <tarp/evchan.hxx>

#include "event_broadcaster_test.hxx"

using namespace std;
using namespace std::chrono_literals;
namespace E = tarp::evchan::ts;

// helper for consumer/producer threads to get notifications.
struct wait_struct : public E::interfaces::notifier{
    tarp::binary_semaphore sem;
    bool notify(uint32_t, uint32_t) override{
        sem.release();
        return true;
    }
};

// Broadcast num_msgs, 1 message per broadcast_period, to a broadcast group
// of num_consumers, each joined with an event_channel with buffer size
// consumer_buffsz.
// An event_broadcaster is lossy, but when we specify a high enough
// broadcast_period, we should see that all events are received by every single
// subscriber.
//
// Note in this test each consumer corresponds to a separate thread; therefore
// since there is a maximum system limit to the number of threads, there is a
// limit to the number of consumers.
bool test_event_broadcaster(
        uint32_t num_msgs,
        std::chrono::microseconds broadcast_period,
        uint32_t num_consumers,
        unsigned consumer_buffsz)
{
    E::event_broadcaster<unsigned, unsigned, unsigned> br(true);
    using consumer_chan_t = E::event_channel<unsigned, unsigned, unsigned>;

    std::vector<std::unique_ptr<std::uint32_t>> consumer_counters;
    std::map<unsigned, std::pair<thread, shared_ptr<consumer_chan_t>>> consumers;

  bool test_passed{true};

  auto start_consumers = [&](){
    for (uint32_t i = 0; i < num_consumers; ++i) {
        std::pair<thread, shared_ptr<consumer_chan_t>> p;
        p.second = make_shared<consumer_chan_t>(consumer_buffsz, true);
        br.connect(p.second);
        auto &chan = p.second->as_rchan();

        consumer_counters.push_back(std::make_unique<uint32_t>(0));
        auto &counter = *consumer_counters.back().get();

      thread t([&chan, &counter]() {
        auto w = make_shared<wait_struct>();
        chan.add_monitor(w);
        //std::cerr << "Consumer " << i << " initialized with counter address " << std::addressof(counter) << std::endl;
        for (;;){
        //cerr << "chan get waiting\n";
            auto data = chan.try_get();
        //cerr << "after chan get waiting\n";
            if (data.has_value()){
                //std::cerr << " Incrementing counter " << addressof(counter) << std::endl;
                ++counter;
            } else if (chan.closed()){
                break;
            } else {
                w->sem.acquire();
            }
        }
      });

      std::swap(p.first, t);
      consumers.insert(std::make_pair(i, std::move(p)));
    }
  };

    auto close_channels = [](auto &ls){
        for (auto &[k, p] : ls){
            auto &[t, ch] = p;
            ch->close();
        }
    };

  auto join_threads = [](auto &ls) {
    for (auto &[k, v] : ls) {
        auto &[t, chan] = v;
        t.join();
    }
  };

  cerr << "starting consumers\n";
  start_consumers();

  std::cerr << "RUNNING" << std::endl;

    auto msg = std::make_tuple(0, 0, 0);
    for (unsigned i = 0; i < num_msgs; ++i){
        br << msg;
        std::this_thread::sleep_for(broadcast_period);
    }

    cerr << "closing channels\n";
    close_channels(consumers);
    join_threads(consumers);

    //
    // PRINTS
    //
    std::cerr << std::dec << std::endl;
    std::cerr << "Asked to broadcast: " << num_msgs << " msgs" << std::endl;

    uint32_t num_msgs_pushed = 0;
    for (auto &i : consumer_counters){
        num_msgs_pushed += *i;
    }

    test_passed = num_msgs_pushed == (num_consumers * num_msgs);
    std::cerr << "Received: " << num_msgs_pushed << " messages across "
        << num_consumers << " consumers)"<< std::endl;

#if 0
    std::cerr << "Consumer counters: " << std::endl;
    for (auto &c: consumer_counters){
        std::cerr << " | " << addressof(c) << " : "<< std::dec << *c << std::endl;
    }
#endif
  return test_passed;
}

