#include "trunk_test.hxx"

#include <iostream>

#include <tarp/evchan.hxx>

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

  run_test(test_channel_closing);
  run_test(test_no_buffering);
  run_test(test_unbuffered_mcsp);
  run_test(test_unbuffered_mpsc);
  run_test(test_try_for_timings);
  run_test(test_trunk_monitor,100, 100, 2000, 10s);
  run_test(test_trunk_interfaces,5000, 2s);

  //run_test(test_benchmark, 2, 20, 100 * 1000 * 1000, 10s);

  cerr << endl;
  cerr << "Passed: " << num_passed << "/" << num_total << "." << endl;

  bool passed = num_passed == num_total;
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
