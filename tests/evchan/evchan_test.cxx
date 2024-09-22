#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tarp/cxxcommon.hxx>
#include <tarp/semaphore.hxx>
#include <thread>
#include <map>

#include <tarp/evchan.hxx>
#include <utility>

#include "evchan_test.hxx"

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

// Create num_producers, each sending at most num_msgs. Create num_consumers
// to receive as many messages of the ones sent, as possible.
// Wait for the messages to flow for max_duration, then stop, regardless
// of whether all messages were sent or not.
bool test_chan_monitor(uint32_t num_producers, uint32_t num_consumers,
        uint32_t num_msgs,
        unsigned producer_buffsz,
        unsigned consumer_buffsz, 
        std::chrono::seconds max_duration)
{
  using producer_payload = unsigned;
  using consumer_payload = std::unique_ptr<std::pair<unsigned, unsigned>>;

  using producer_chan_t = E::event_channel<producer_payload>;
  using consumer_chan_t = E::event_channel<consumer_payload>;
  std::map<unsigned, std::pair<thread, std::unique_ptr<consumer_chan_t>>> consumers;
  std::map<unsigned, std::pair<thread, std::unique_ptr<producer_chan_t>>> producers;

  std::vector<std::unique_ptr<std::uint32_t>> consumer_counters;
  std::uint32_t num_producers_running = num_producers;

  bool test_passed{true};

  auto start_producers = [&](){
    for (uint32_t i = 0; i < num_producers; ++i) {
        std::pair<thread, unique_ptr<producer_chan_t>> p;
        p.second = make_unique<producer_chan_t>(producer_buffsz, false);
        auto &chan = p.second->as_wchan();

        thread t([&]() {
            auto w = make_shared<wait_struct>();
            chan.add_monitor(w);
            for (uint32_t j = 0; j < num_msgs; ++j){
                auto [ok, data] = chan.try_push(j);

                if (chan.closed()){
                    break;
                }

                if (!ok){
                    --j; // retry
                    w->sem.acquire(); // wait for writability notification.
                }
            }

            //std::cerr << "* * * writer done\n";
        --num_producers_running;
      });

      std::swap(p.first, t);
      producers.insert(std::make_pair(i, std::move(p)));
    }
  };

  auto start_consumers = [&](){
    for (uint32_t i = 0; i < num_consumers; ++i) {
        std::pair<thread, unique_ptr<consumer_chan_t>> p;
        p.second = make_unique<consumer_chan_t>(consumer_buffsz, false);
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
      auto k = i+num_producers;
      consumers.insert(std::make_pair(k, std::move(p)));
    }
  };

  auto join_threads = [](auto &ls) {
    for (auto &[k, v] : ls) {
        auto &[t, chan] = v;
        t.join();
    }
  };

    auto monitor_channels = [](auto &ls, auto &monitor, auto flags, unsigned base_key){
        auto k = base_key; 
        for (auto &[_, v] : ls){
            auto &[t, chan]  = v;
            //cerr << "watching channel " << k << " flags=" << flags << endl;
            monitor.watch(*chan, flags, k);
            ++k;
        }
    };

  auto start_time = std::chrono::steady_clock::now();
  auto deadline = start_time + max_duration;
  start_consumers();
  start_producers();

  std::cerr << "RUNNING" << std::endl;

  // all channels with key < (0+num_producers) are producer channels; all
  // channels with key >= than this are consumer channels.
  E::monitor mon;
  monitor_channels(producers, mon, E::chanState::READABLE, 0);
  monitor_channels(consumers, mon, E::chanState::WRITABLE, 0 + num_producers);

  std::list<unsigned> buff;
  std::uint32_t num_buffered{0};


  while(std::chrono::steady_clock::now() < deadline){
    auto evs = mon.wait_until(deadline);
    //cerr << "\n\nUNBLOCKED\n";
    for (auto [k, mask]: evs){
        //std::cerr << "::chan=" << k << ", events=" << mask << endl;

        if (mask & E::chanState::CLOSED){
            break;
        }

        if (k < 0+num_producers){
            if (mask & E::chanState::READABLE){
                auto &[t, chan] = producers[k];
                auto res = chan->try_get();
                if (res.has_value()){
                    buff.push_back(*res);
                    ++num_buffered;
                    //cerr << "buffered1\n";
                }else{
                    std::cerr << "failed get on chan " << k << std::endl;
                }
            }
        } else if (k >= 0+num_producers && k < 0+num_producers+num_consumers){
            //cerr << "consumers !!!!!!!!!!!... " << endl;
            auto &[t, chan] = consumers[k];
            if (mask & E::chanState::WRITABLE){
                if (!buff.empty()){
                    auto item = buff.front();
                    buff.pop_front();

                    auto data = std::make_unique<std::pair<unsigned, unsigned>>(item, item+1);
                    THROW_IF_NULL(chan);
                    auto [ok, d] = chan->try_push(std::move(data));
                    //cerr <<"pushed1 ? " << pushed << std::endl;
                    if (!ok){
                        cerr << "failed to push on channel " << k << std::endl;
                        // put it back in the list so we can retry
                        auto &ptr = *d;
                        THROW_IF_NULL(ptr);
                        buff.push_front(std::move(ptr->first));
                        break;
                    } else {
                        //cerr << "good push on channel " << k << std::endl;
                    }
                }
            }
        }
    }
  }

    std::cerr << "after loop" << std::endl;
    auto elapsed = std::chrono::steady_clock::now() - start_time;

    std::cerr << "closing channel" << std::endl;
    auto close_channels = [](auto &ls){
        for (auto &[k, p] : ls){
            auto &[t, ch] = p;
            ch->close();
        }
    };
    close_channels(consumers);
    close_channels(producers);

    std::cerr << "joining consumers" << std::endl;
    join_threads(consumers);
    std::cerr << "joining producers" << std::endl;
    join_threads(producers);
    std::cerr << "all done" << std::endl;

    //
    // PRINTS
    //
    std::cerr << std::dec << std::endl;
    std::cerr << "Asked to push: " << num_msgs << " messages (per producer)" << std::endl;

    uint32_t num_msgs_pushed = 0;
    for (auto &i : consumer_counters){
        num_msgs_pushed += *i;
    }

    test_passed = num_msgs_pushed == (num_producers * num_msgs);
    std::cerr << "Pushed: " << num_msgs_pushed << " messages in total ("
        << num_producers << " producers)"<< std::endl;

    std::cerr << "Buffered: " << num_buffered << std::endl;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    std::cerr << "Time elapsed: " << duration.count() << " ms" << std::endl;

    std::cerr << "Consumer counters: " << std::endl;
    for (auto &c: consumer_counters){
        std::cerr << " | " << addressof(c) << " : "<< std::dec << *c << std::endl;
    }
  return test_passed;
}

// Create a single sender and receiver and connect them on the same trunk.
// The sender uses a wtrunk and the receiver uses an trunk.
// NUM msgs are passed from sender to receiver.
bool test_chan_interfaces(unsigned num_msgs, unsigned chan_capacity, std::chrono::seconds duration)
{
    auto channel = make_shared<E::event_channel<unsigned, bool>>(chan_capacity, false);

    using wchan_type = shared_ptr<decltype(channel)::element_type::wchan_t>;
    using rchan_type = shared_ptr<decltype(channel)::element_type::rchan_t>;

    uint32_t num_msgs_received = 0;
    
    auto run_sender = [&num_msgs](std::thread &t, wchan_type chan){
    //auto run_sender = [&num_msgs](std::thread &t, auto &chan){
        std::thread th([&, chan]{
            auto w = make_shared<wait_struct>();
            chan->add_monitor(w);

            for (unsigned i = 0; i  < num_msgs; ++i){
                if (!chan){
                    throw std::logic_error("null");
                }
                auto [ok, data] = chan->try_push(i, true);

                if (chan->closed()){
                    break;
                }

                if (!ok){
                    --i; // retry
                    w->sem.acquire();
                }
            }
        });
        std::swap(t, th);
    };

    auto run_receiver = [&num_msgs_received](
            std::thread &t, rchan_type chan)
    {
        std::thread th([&, chan]{
            auto w = make_shared<wait_struct>();
            chan->add_monitor(w);

            for (;;){
                auto data = chan->try_get();
                if (data.has_value()){
                    ++num_msgs_received;
                } else if (chan->closed()){
                    break;
                }
                else {
                    // wait for readability signal.
                    w->sem.acquire();
                }
            }
        });
        std::swap(th, t);
    };

    std::thread sending_thread;
    std::thread receiving_thread;

    // notice how the trunk is implicitly upcast to an rtrunk/wtrunk
    run_sender(sending_thread, channel);
    run_receiver(receiving_thread, channel);

    std::this_thread::sleep_for(duration);
    channel->close();
    sending_thread.join();
    receiving_thread.join();

    std::cerr << "Receiver got " << num_msgs_received << " messages\n";
    return num_msgs == num_msgs_received;
}


