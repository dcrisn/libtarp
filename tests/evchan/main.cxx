#include <iostream>

#include <tarp/evchan.hxx>

#include "trunk_test.hxx"
#include "evchan_test.hxx"
#include "event_broadcaster_test.hxx"
#include "event_aggregator_test.hxx"
#include "event_stream_test.hxx"

using namespace std;
using namespace std::chrono_literals;


int main(int, const char **) {
  unsigned num_test{0};
  unsigned num_total{0};
  unsigned num_passed{0};

  auto run_test = [&](auto &&test_func, auto &&...args) {
    ++num_total;
    ++num_test;
    cerr << "\n=== Starting test === " << num_test << endl;
    bool ok = test_func(std::forward<decltype(args)>(args)...);
    std::cerr << "Test " << num_test << " " << (ok ? "passed" : "failed")
              << endl;

    if (ok) {
      ++num_passed;
    }
  };

  //==========================
  // ===== Test class `trunk`
  //==========================

  run_test(test_channel_closing);
  run_test(test_no_buffering);
  run_test(test_unbuffered_mcsp);
  run_test(test_unbuffered_mpsc);
  run_test(test_try_for_timings);
  run_test(test_trunk_monitor,100, 100, 2000, 10s);
  run_test(test_trunk_interfaces,5000, 2s);

  //run_test(test_benchmark, 2, 20, 100 * 1000 * 1000, 10s);


  //==========================
  // ===== Test class `evchan`
  //==========================

  // NOTE:" "if you want to benchmark troughput, increase the number of messages to a
  // great value so that the deadline is reached before the work is finished.
  // Conversely, to check that thew test passes, decrease the number of messages
  // so we can see all messages are pushed before the deadline is reached.
  run_test(test_chan_monitor, 16, 18, 10 *1000, 10, 10, 10s);

  //NOTE: somewhat surprisingly, increasing the length of the event buffer
  //dramatically increases performance. From 800k per second with a buffsz=1
  //to 8 million per second.
  run_test(test_chan_interfaces,1000 * 1000,  500, 10s);

  //=====================================
  // ===== Test class `event_broadcaster`
  //=====================================
  run_test(test_event_broadcaster, 1000, 10ms, 1 * 1000, 1);

  //=====================================
  // ===== Test class `event_aggregator`
  //=====================================
  // NOTE: we make the per-producer channel buffer big enough to prevent any
  // event being lost whatsoever -- just for the purpose of the test, so
  // it does not fail because of fast senders outpacing the sole receiver.
  run_test(test_event_aggregator, 1000, 500, 1000, 10s, 100us);

  //=======================================
  // ===== Test classes `event_{r,w}stream`
  //=======================================

  // NOTE: we make the buffer big enough for all the messages we'll ever get
  // in order  -- just for the sake of the test -- avoid losing any messages.
  // Otherwise the senders overwhelm the sole receiver and messages will get
  // dropped once the buffer is filled to capacity, and the test fails.
  run_test(test_event_wstream, 1000, 500, 1000 * 500, 10s, 100us);

  run_test(test_event_rstream, 10 * 1000, 100us, 1 * 100, 10);

  cerr << endl;
  cerr << "Passed: " << num_passed << "/" << num_total << "." << endl;

  bool passed = num_passed == num_total;
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
