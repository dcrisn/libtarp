#include "trunk_test.hxx"
#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <thread>

#include <tarp/evchan.hxx>
#include <utility>

using namespace std;
using namespace std::chrono_literals;
namespace E = tarp::evchan::ts;

template <typename F> void loop(unsigned passes, F &&f) {
  for (unsigned i = 0; i < passes; ++i) {
    f(i);
  }
}

// Create a signle consumers and a bunch of producers. All use
// the same trunk. When close() is called, they all should unblock.
// The close() should be pretty quick e.g. ~20ms and not hang too long.
bool test_channel_closing() {
  struct mystruct {};

  E::trunk<std::unique_ptr<struct mystruct>> trunk;
  std::vector<std::unique_ptr<struct mystruct>> items;

  constexpr size_t NUM_PRODUCERS{100};
  std::array<std::thread, NUM_PRODUCERS> producers;

  // num items per producer
  constexpr size_t NUM_ITEMS{100};

  auto start_producers = [&producers, &trunk]() {
    for (auto &producer : producers) {
      std::thread t([&trunk] {
        for (unsigned i = 0; i < NUM_ITEMS; ++i) {
          auto item = std::make_unique<struct mystruct>();
          if (auto [ok, data] = trunk.push(std::move(item)); !ok) {
            break; /* close() called */
          }
        }
      });
      swap(t, producer);
    }
  };

  start_producers();

  constexpr chrono::milliseconds SLEEP_TIME{100ms};
  std::thread consumer([&trunk, &items, &SLEEP_TIME]() {
    while (true) {
      std::this_thread::sleep_for(SLEEP_TIME);
      auto o = trunk.get();
      if (!o.has_value()) {
        break;
      }
      items.push_back(std::move(*o));
    }
  });

  // sice we sleep for 5s and receive at an iterval of 100ms,
  // by the end we should've received close to 50 (if not exactly
  // 50) messages.
  auto WAIT_DURATION{5s};
  std::this_thread::sleep_for(WAIT_DURATION);

  std::cerr << "Closing channel" << endl;
  auto start = std::chrono::steady_clock::now();
  trunk.close();
  std::cerr << "After close" << endl;

  for (auto &t : producers) {
    t.join();
  }
  consumer.join();

  // We should've dequeued at least a few dozen items
  std::cerr << "Messages received : " << items.size() << endl;
  if (items.empty()) {
    return false;
  }

  // once we called close() on the channel, all senders and receivers should've
  // ublocked and their threads exited quickly.
  auto duration = std::chrono::steady_clock::now() - start;
  std::cerr
      << "Close() took "
      << std::chrono::duration_cast<chrono::milliseconds>(duration).count()
      << "ms to take effect" << endl;
  auto overhead = 20ms;
  if (duration > SLEEP_TIME + overhead) {
    return false;
  }

  return true;
}

// Using an unbuffered channel (i.e. trunk), no try_push should succeed if
// there is no corresponding get(). And no try_get() should succeed after
// the fact, when there is no corresponding push, since nothing should've been
// buffered inside the channel.
bool test_no_buffering() {
  E::trunk<string, unsigned> chan;
  unsigned num_items{10000};
  bool test_passed{true};

  std::thread t1([&chan, num_items, &test_passed] {
    for (unsigned i = 0; i < num_items; ++i) {
      auto [ok, data] = chan.try_push_for(1us, "t1"s, i);
      if (ok){
        test_passed = false;
        return;
      }
    }
  });

  std::thread t2([&chan, num_items, &test_passed] {
    for (unsigned i = 0; i < num_items; ++i) {
      auto [ok, data] = chan.try_push_until(std::chrono::steady_clock::now() + 10us,
                          tuple{"t3"s, i});
      if (ok){
        test_passed = false;
        return;
      }
    }
  });

  std::thread t3([&chan, num_items, &test_passed] {
    for (unsigned i = 0; i < num_items; ++i) {
        auto [ok, data] = chan.try_push("t2", i);
        if (ok){
            test_passed = false;
            return;
        }
      std::this_thread::sleep_for(2us);
    }
  });

  t1.join();
  t2.join();
  t3.join();

  for (unsigned i = 0; i < num_items; ++i) {
    auto res = chan.try_get_for(5us);
    if (res.has_value()) {
      test_passed = false;
      // auto &[str, u] = *res;
      // std::cerr << "Got: " << str << ", " << u << std::endl;
    }
  }

  if (test_passed){
      cerr << "0 push() calls expected to succeed; 0 succeeded." << endl;
  }
  return test_passed;
}

// When we have multiple consumers reading from the channel and a single
// producer, then all of the pushes should succeed. Each consumer should be
// getting roughly the same number of receives in.
bool test_unbuffered_mcsp() {
  E::trunk<unsigned> chan;

  unsigned num_items{100'000};
  bool writer_done{false};

  constexpr unsigned NUM_CONSUMERS{10};
  // consumers get events and put them int their own queue
  std::array<std::vector<decltype(chan)::payload_t>, NUM_CONSUMERS> cqs;
  std::array<thread, NUM_CONSUMERS> cths;

  auto start_consumer = [&writer_done, &chan](auto &th, auto &q) {
    thread t{[&writer_done, &chan, &q] {
      while (!writer_done) {
        auto res = chan.try_get_for(1ms);

        if (res.has_value()) {
          q.push_back(res.value());
        }
      }
    }};

    std::swap(t, th);
  };

  // start producer
  std::thread p{[&num_items, &chan, &writer_done] {
    for (unsigned i = 0; i < num_items; ++i) {
      //std::cerr << "pushing..." << std::endl;
      chan.push(i);
      //std::cerr << "after push..." << std::endl;
    }
    writer_done = true;
  }};

  // start consumers
  for (unsigned i = 0; i < NUM_CONSUMERS; ++i) {
    start_consumer(cths[i], cqs[i]);
  }

  while (!writer_done) {
    std::this_thread::sleep_for(1ms);
  }

  p.join();
  for (auto &i : cths) {
    i.join();
  }

  // These should be pretty close to each other, otherwise it's probably
  // wrong! and they of course must add up to num_items
  std::cerr << "Consumer queues: " << endl;
  auto total{0u};
  for (unsigned i = 0; i < NUM_CONSUMERS; ++i) {
    cerr << "C" << i << ": " << cqs[i].size() << endl;
    total += cqs[i].size();
  }

  if (total != num_items) {
    return false;
  }

  return true;
}

// When we have a single consumer and multiple producers, then the producers
// are only able to push when the consumer gets around to them. If we have N
// producers each equeueing items 1..n, (and the cosumer calls get() up until
// this many items are dequeued), then we should be able to build a multiset
// from all the items dequeued with size N*n: n unique items, and each unique
// item occuring N times.
bool test_unbuffered_mpsc() {
  E::trunk<unsigned> chan;

  unsigned num_items{20};

  constexpr unsigned NUM_PRODUCERS{500};
  std::array<thread, NUM_PRODUCERS> pths;

  auto start_producer = [&chan, &num_items](auto &th) {
    thread t{[&num_items, &chan] {
      for (unsigned i = 0; i < num_items; ++i) {
        chan << i;
      }
    }};

    std::swap(t, th);
  };

  // start consumers
  for (auto &p : pths) {
    start_producer(p);
  }

  // run consumer
  unsigned passes_required{0};
  std::vector<decltype(chan)::payload_t> q;
  while (q.size() < num_items * NUM_PRODUCERS) {
    passes_required++;
    for (unsigned j = 0; j < NUM_PRODUCERS; ++j) {
      // std::cerr << "calling try_get " << std::endl;
      auto res = chan.try_get();
      // std::cerr << "after calling try_get " << std::endl;
      if (res.has_value()) {
        q.push_back(res.value());
      }
    }
  }

  cerr << "Dequeued all events after " << passes_required << " passes" << endl;

  // std::cerr << "BLOCKED in join" << endl;
  for (auto &i : pths) {
    i.join();
  }

  std::cerr << "Consumer queue: size=" << q.size() << endl;
  if (q.size() != num_items * NUM_PRODUCERS) {
    return false;
  }

  // for (auto i : q) {
  //   cerr << i << endl;
  // }

  // We should have num_items unique values in the queue. And each
  // unique value should appear (have a count of) NUM_PRODUCERS.
  std::multiset<unsigned> set(q.begin(), q.end());
  for (auto i : set) {
    if (set.count(i) != NUM_PRODUCERS) {
      cerr << "Bad count of set entry " << endl;
      return false;
    }
  }

  return true;
}

// Test that try_get and try_push work as expected. That is, call
// try_get/try_push with a certain specified duration, and only
// call the corresponding get()/push() that will unblock the sender
// (blocked in a call to push()), toward the end of the duration.
// All the calls should succeed; However, if they do not, it is
// very likely because of the multithreaded scheduler.
//
// We are also sending some more complicated structures here for the sake of
// testing.
bool test_try_for_timings() {
  constexpr unsigned NUM_PRODUCERS{1};
  constexpr unsigned NUM_CONSUMERS{NUM_PRODUCERS};

  // Just to make it more complicated and see that compilation works;
  // we are only really interested in the first item (i.e. tuple[0]).
  using payload_type =
      tuple<unsigned, std::unique_ptr<pair<string, string>>, uint16_t>;

  E::trunk<payload_type> chan;

  array<thread, NUM_CONSUMERS> consumers;
  array<thread, NUM_CONSUMERS> producers;
  array<vector<payload_type>, NUM_CONSUMERS> consumer_qs;

  bool test_passed{true};

  auto start_producers = [&test_passed, &producers, &chan](const auto &duration,
                                                           bool try_mode,
                                                           unsigned value) {
    for (auto &i : producers) {
      thread t([try_mode, duration, &test_passed, &chan, value]() {
        payload_type data{value, nullptr, 0};

        // If try mode then TRY a call for the specified duration
        if (try_mode) {
          //cerr << "Calling try_push_for" << endl;
          if (auto [ok, returned] = chan.try_push_for(duration, std::move(data)); !ok) {
            test_passed = false;
          }

          //cerr << "after try_push_for" << endl;
          return;
        }

        // ELSE sleep for the specified duration THEN make a direct blocking
        // call (push/get).
        this_thread::sleep_for(duration);
        //cerr << "Calling push()" << endl;
        chan.push(std::move(data));
        //cerr << "After calling push()" << endl;
      });

      swap(i, t);
    }
  };

  auto start_consumers = [&test_passed, &consumers, &chan](const auto &duration,
                                                           bool try_mode,
                                                           unsigned value) {
    for (auto &i : consumers) {
      thread t([try_mode, duration, &test_passed, &chan, value]() {
        //
        if (try_mode) {
          //cerr << "Calling try_get_for" << endl;
          auto res = chan.try_get_for(duration);
          //cerr << "After calling try_get_for" << endl;
          if (!res.has_value()) {
            test_passed = false;
            return;
          }

          // check that the value we get is the one we expect (the one we
          // passed in)
          if (get<0>(*res) != value) {
            test_passed = false;
          }
          return;
        }

        // else sleep for duration, then call the non-try version.
        std::this_thread::sleep_for(duration);
        //cerr << "Calling get" << endl;
        auto data = chan.try_get();
        //cerr << "After calling get" << endl;
        if (!data.has_value() or get<0>(*data) != value) {
          test_passed = false;
        }
      });

      swap(i, t);
    }
  };

  auto join_threads = [](auto &threads) {
    for (auto &t : threads) {
      t.join();
    }
  };

  // durations to try; this is a list of pairs, where p.first is the
  // duration that should be passed to try_push or try_get, and p.second
  // is the delay after which the corresponding get() or push() should be
  // made. Obviously p.second should be smaller than p.first so that the
  // try_get or try_push succeeds. That is, we want to delay the push/get
  // call that causes the try_* call to unblock by making it when the try_*
  // call is close to its expiratio.
  // But we should make the delay and the try_* durationn fairly close to
  // each other so that we can check the timing works as expected.
  // Cannot make them too close however, since then whether the test
  // passes will depend to a certain degree on when the OS scheduled
  // the specific thread to run.
  vector<pair<chrono::microseconds, chrono::microseconds>> durations{
      {500us, 10us},  {1ms, 800us},    {2ms, 1ms},
      {10ms, 999us},  {50ms, 49600us}, {100ms, 99ms},
      {200ms, 199ms}, {1s, 900ms},     {2s, 1999ms}};

  unsigned value{0};

  // For each duration pair, engage the producers in try_push calls
  // for the given p.first duration. Then close to when this duration would
  // expire, for each producer start a consumer that makes a get() call,
  // which should make the corresponding try_push succeed. Then do the reverse.
  for (auto &[try_duration, holdback_duration] : durations) {
    if (!test_passed) {
      break;
    }

    ++value;
    chrono::microseconds duration_try{try_duration};
    chrono::microseconds duration_holdback{holdback_duration};

    cerr << "Testing with try_duration=" << duration_try.count()
         << "us and holdback_duration=" << duration_holdback.count() << "us"
         << endl;

    start_producers(try_duration, true, value);
    start_consumers(holdback_duration, false, value);

    join_threads(consumers);
    join_threads(producers);
  }

  return test_passed;
}

// Test the performance of the implementation. The run can be tweaked by
// increasing the number of producers, consumers, messages passed, etc.
// NOTE: performance gets worse the more producers there are. 1 producer
// performs best. The reason is we do not simulate doing any work to generate the
// message. We simply push a message immediately, so *all* overhead is due to
// thread management by the OS. Therefore the more threads there are, naturally,
// the slower things get. But you would not use multiple threads in this
// situation! You want to use threads to take advantage of multiple cores i.e. when
// performance is **CPU-bound**. In this example, however, we do not do any processing
// so we are not CPU-bound. In this case, one thread is ideal.
bool test_benchmark(uint32_t num_producers, uint32_t num_consumers,
        uint32_t num_msgs, std::chrono::seconds max_duration)
{
  using payload_type = std::pair<uint32_t, std::unique_ptr<uint32_t>>;

  E::trunk<payload_type> chan;

  std::vector<thread> consumers;
  std::vector<thread> producers;
  std::vector<std::unique_ptr<std::uint32_t>> consumer_counters;
  std::uint32_t num_producers_running = num_producers;

  bool test_passed{true};

  auto start_producers = [&](){
    for (uint32_t i = 0; i < num_producers; ++i) {
      thread t([&chan, &num_msgs, &num_producers_running]() {
        for (uint32_t j = 0; j < num_msgs; ++j){
            auto data = std::make_pair(j, std::make_unique<uint32_t>(j));
            chan.push(std::move(data));

            if (chan.closed()){
                break;
            }
        }
        --num_producers_running;
      });

      producers.emplace_back(std::move(t));
    }
  };

  auto start_consumers = [&](){
    for (uint32_t i = 0; i < num_consumers; ++i) {
        consumer_counters.push_back(std::make_unique<uint32_t>(0));
        auto &counter = *consumer_counters.back().get();

      thread t([&chan, &counter]() {
        //std::cerr << "Consumer " << i << " initialized with counter address " << std::addressof(counter) << std::endl;
        for (;;){
            auto data = chan.get();
            if (data.has_value()){
                //std::cerr << " Incrementing counter " << addressof(counter) << std::endl;
                ++counter;
            }
            if (chan.closed()){
                break;
            }
        }
      });

      consumers.emplace_back(std::move(t));
    }
  };

  auto join_threads = [](auto &threads) {
    for (auto &t : threads) {
      t.join();
    }
  };

  auto start_time = std::chrono::steady_clock::now();
  auto deadline = start_time + max_duration;
  start_consumers();
  start_producers();

  std::cerr << "RUNNING" << std::endl;
    while (num_producers_running > 0 && deadline > std::chrono::steady_clock::now())
    {
        std::this_thread::sleep_for(1ms);
    }

    std::cerr << "after loop" << std::endl;

    auto elapsed = std::chrono::steady_clock::now() - start_time;

    std::cerr << "closing channel" << std::endl;
    chan.close();

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

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    std::cerr << "Time elapsed: " << duration.count() << " ms" << std::endl;

    std::cerr << "Consumer counters: " << std::endl;
    for (auto &c: consumer_counters){
        std::cerr << " | " << addressof(c) << " : "<< std::dec << *c << std::endl;
    }

  return test_passed;
}

// Create num_producers, each sending at most num_msgs. Create num_consumers
// to receive as many messages of the ones sent, as possible.
// Monitor the producers for readability and the consumers for writability,
// and help transport messages from producers to consumers. PASS if all
// messages produced are successfully passed to consumers.
bool test_trunk_monitor(uint32_t num_producers, uint32_t num_consumers,
        uint32_t num_msgs, std::chrono::seconds max_duration)
{
  using producer_payload = unsigned;
  using consumer_payload = std::unique_ptr<std::pair<unsigned, unsigned>>;


  using producer_chan_t = E::trunk<producer_payload>;
  using consumer_chan_t = E::trunk<consumer_payload>;
  std::map<unsigned, unique_ptr<std::pair<thread, consumer_chan_t>>> consumers;
  std::map<unsigned, unique_ptr<std::pair<thread, producer_chan_t>>> producers;

  std::vector<std::unique_ptr<std::uint32_t>> consumer_counters;
  std::uint32_t num_producers_running = num_producers;


  bool test_passed{true};

  auto start_producers = [&](){
    for (uint32_t i = 0; i < num_producers; ++i) {
        auto prod = make_unique<pair<thread, producer_chan_t>>();
        auto &pair = *prod;
        producers.insert(std::make_pair(i,  std::move(prod)));

        thread t([&, &chan=pair.second.as_wtrunk()]() {
            for (uint32_t j = 0; j < num_msgs; ++j){
                chan.push(j);

                if (chan.closed()){
                    break;
                }
            }

            //std::cerr << "* * * writer done\n";
        --num_producers_running;
      });

        std::swap(pair.first, t);
    }
  };

  auto start_consumers = [&](){
    for (uint32_t i = 0; i < num_consumers; ++i) {
        auto k = i+num_producers;

        auto cons = make_unique<pair<thread, consumer_chan_t>>();
        auto &pair = *cons;
        consumers.insert({k, std::move(cons)});

        consumer_counters.push_back(std::make_unique<uint32_t>(0));
        auto &counter = *consumer_counters.back().get();

      thread t([&test_passed, &chan=pair.second.as_rtrunk(), &counter]() {
        //std::cerr << "Consumer " << i << " initialized with counter address " << std::addressof(counter) << std::endl;
        for (;;){
        //cerr << "chan get waiting\n";
            auto data = chan.get();
        //cerr << "after chan get waiting\n";
            if (data.has_value()){
                //std::cerr << " Incrementing counter " << addressof(counter) << std::endl;
                ++counter;
            }
            if (chan.closed()){
                break;
            }
        }
      });

      std::swap(pair.first, t);
    }
  };

  auto join_threads = [](auto &ls) {
    for (auto &[k, v] : ls) {
        auto &[t, chan] = *v;
        t.join();
    }
  };

    auto monitor_channels = [](auto &ls, auto &monitor, auto flags, unsigned base_key){
        auto k = base_key; 
        for (auto &[_, v] : ls){
            auto &[t, chan]  = *v;
            monitor.watch(chan, flags, k);
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
                auto &[t, chan] = *producers[k];
                auto res = chan.try_get();
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
            auto &[t, chan] = *consumers[k];
            if (mask & E::chanState::WRITABLE){
                if (!buff.empty()){
                    auto item = buff.front();
                    buff.pop_front();

                    auto data = std::make_unique<std::pair<unsigned, unsigned>>(item, item+1);
                    auto [ok, d] = chan.try_push(std::move(data));
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

    //std::cerr << "after loop" << std::endl;
    auto elapsed = std::chrono::steady_clock::now() - start_time;

    //std::cerr << "closing channel" << std::endl;
    auto close_channels = [](auto &ls){
        for (auto &[k, p] : ls){
            auto &[t, ch] = *p;
            ch.close();
        }
    };
    close_channels(consumers);
    close_channels(producers);

    //std::cerr << "joining consumers" << std::endl;
    join_threads(consumers);
    //std::cerr << "joining producers" << std::endl;
    join_threads(producers);
    //std::cerr << "all done" << std::endl;

    //
    // PRINTS
    //
    std::cerr << std::dec << std::endl;
    std::cerr << "Asked to push: " << num_msgs << " messages (per producer)" << std::endl;

    uint32_t num_msgs_read = 0;
    for (auto &i : consumer_counters){
        num_msgs_read += *i;
    }

    test_passed = num_msgs_read == (num_producers * num_msgs);
    std::cerr << "Pushed: " << num_msgs_read << " messages in total ("
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
// The sender uses a wtrunk and the receiver uses an rtrunk.
// NUM msgs are passed from sender to receiver.
// PASS if the receiver received NUM msgs.
bool test_trunk_interfaces(unsigned num_msgs, std::chrono::seconds duration){
    auto trunk = make_shared<E::trunk<unsigned, bool>>();

    using wtrunk_type = shared_ptr<decltype(trunk)::element_type::wtrunk_t>;
    using rtrunk_type = shared_ptr<decltype(trunk)::element_type::rtrunk_t>;

    uint32_t num_msgs_received = 0;
    
    auto run_sender = [&num_msgs](std::thread &t, wtrunk_type chan){
    //auto run_sender = [&num_msgs](std::thread &t, auto &chan){
        std::thread th([&, chan]{
            for (unsigned i = 0; i  < num_msgs; ++i){
                if (!chan){
                    throw std::logic_error("null");
                }
                chan->push(i, true);
                if (chan->closed()){
                    break;
                }
            }
        });
        std::swap(t, th);
    };

    auto run_receiver = [&num_msgs, &num_msgs_received](
            std::thread &t, rtrunk_type chan)
    {
        std::thread th([&, chan]{
            for (unsigned i = 0; i  < num_msgs; ++i){
                auto data = chan->get();
                if (data.has_value()){
                    ++num_msgs_received;
                }
                if (chan->closed()){
                    break;
                }
            }
        });
        std::swap(th, t);
    };

    std::thread sending_thread;
    std::thread receiving_thread;

    // notice how the trunk is implicitly upcast to an rtrunk/wtrunk
    run_sender(sending_thread, trunk);
    run_receiver(receiving_thread, trunk);

    std::this_thread::sleep_for(duration);
    trunk->close();
    sending_thread.join();
    receiving_thread.join();

    std::cerr << "Receiver got " << num_msgs_received << " messages\n";
    return num_msgs == num_msgs_received;
}

