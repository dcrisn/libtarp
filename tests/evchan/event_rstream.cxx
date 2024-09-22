#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <thread>
#include <map>
#include <utility>

#include <tarp/evchan.hxx>

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

// Stream num_msgs, 1 message per stream_period, to an anycast group
// of num_consumers. The underlying stream channel is made to have buffsz.
// An event_broadcaster is lossy, but when we specify a high enough
// period, we should see that all events are received by every single
// subscriber.
//
// Note in this test each consumer corresponds to a separate thread; therefore
// since there is a maximum system limit to the number of threads, there is a
// limit to the number of consumers.
//
// You will notice here the performance is not particularly good, and it gets
// worse and worse the more consumers there are. The likely reason is that
// each consumer adds a monitor and every time the channel becomes readable it
// must notify all the monitors. IOW, for every single message, we have to do
// loop over all monitors and lock and unlock mutexes there and whatnot.
bool test_event_rstream(
        uint32_t num_msgs,
        std::chrono::microseconds stream_period,
        uint32_t num_consumers,
        unsigned buffsz)
{
    E::event_rstream<std::unique_ptr<unsigned>> s(true, buffsz);

    std::vector<std::unique_ptr<std::uint32_t>> consumer_counters;
    std::map<unsigned, thread> consumers;

  bool test_passed{true};

  auto start_consumers = [&](){
    for (uint32_t i = 0; i < num_consumers; ++i) {
        auto chan = s.interface().channel();
        consumer_counters.push_back(std::make_unique<uint32_t>(0));
        auto &counter = *consumer_counters.back().get();

      thread t([chan, &counter]() {
        auto w = make_shared<wait_struct>();
        chan->add_monitor(w);
        //std::cerr << "Consumer " << i << " initialized with counter address " << std::addressof(counter) << std::endl;
        for (;;){
        //cerr << "chan get waiting\n";
            auto data = chan->try_get();
        //cerr << "after chan get waiting\n";
            if (data.has_value()){
                //std::cerr << " Incrementing counter " << addressof(counter) << std::endl;
                ++counter;
            } else if (chan->closed()){
                break;
            } else {
                w->sem.acquire();
            }
        }
      });

      consumers.insert(std::make_pair(i, std::move(t)));
    }
  };

  auto join_threads = [](auto &ls) {
    for (auto &[k, t] : ls) {
        //cerr << "joining thread " << k <<endl;
        t.join();
    }
  };

  cerr << "starting consumers\n";
  start_consumers();

  std::cerr << "RUNNING" << std::endl;

    for (unsigned i = 0; i < num_msgs; ++i){
        auto msg = std::make_unique<unsigned>(i);
        s << std::move(msg);
        //cerr << "sleeping for us=" << stream_period.count() << endl;
        std::this_thread::sleep_for(stream_period);
    }

    //cerr << "closing channels\n";
    s.close();

    cerr << "joining threads\n";
    join_threads(consumers);

    //
    // PRINTS
    //
    std::cerr << std::dec << std::endl;
    std::cerr << "Asked to stream: " << num_msgs << " msgs" << std::endl;

    uint32_t num_msgs_pushed = 0;
    for (auto &i : consumer_counters){
        num_msgs_pushed += *i;
    }

    test_passed = num_msgs_pushed == num_msgs;
    std::cerr << "Received: " << num_msgs_pushed << " messages across "
        << num_consumers << " consumers)"<< std::endl;

#if 1
    std::cerr << "Consumer counters: " << std::endl;
    for (auto &c: consumer_counters){
        std::cerr << " | " << addressof(c) << " : "<< std::dec << *c << std::endl;
    }
#endif
  return test_passed;
}

