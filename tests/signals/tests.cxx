#include <cmath>
#include <exception>
#include <memory>
#include <stdexcept>
#include <tarp/cohort.h>
#include <tarp/common.h>
#include <tarp/log.h>
#include <tarp/signal.hxx>
#include <tarp/type_traits.hxx>

#include <iostream>

using namespace std;
using namespace tarp;

template<typename ts_policy, typename callback_signature>
using S =
  std::conditional_t<std::is_same_v<ts_policy, tarp::type_traits::thread_safe>,
                     tarp::ts::monosignal<callback_signature>,
                     tarp::tu::monosignal<callback_signature>>;

namespace monosignal {

#if 1
template<typename ts_policy>
enum testStatus test_unconnected_invocation() {
    static_assert(std::is_same_v<ts_policy, tarp::type_traits::thread_safe> or
                  std::is_same_v<ts_policy, tarp::type_traits::thread_unsafe>);

    {
        S<ts_policy, int(int, int, int)> sig;
        sig.emit(1, 2, 3);
    }

    {
        S<ts_policy, void()> sig;
        sig.emit();
    }

    {
        S<ts_policy, void()> sig;
        sig.emit();
    }

    {
        S<ts_policy, void()> sig;
        sig.emit();
    }

    return testStatus::TEST_PASS;
}

template<typename ts_policy>
enum testStatus test_invocation() {
    {
        const int expected = 11;
        int actual = 0;
        S<ts_policy, int(int, int, int)> sig;
        sig.connect_detached([&](auto a, auto b, auto c) {
            return a + b + c;
        });

        actual = sig.emit(2, 8, 1);

        if (actual != expected) {
            return TEST_FAIL;
        }
    }

    {
        const int expected = 11;
        int actual = 0;
        S<ts_policy, int(void)> sig;
        sig.connect_detached([=]() {
            return expected;
        });

        actual = sig.emit();
        if (actual != expected) {
            return TEST_FAIL;
        }
    }

    return testStatus::TEST_PASS;
}

template<typename ts_policy>
enum testStatus test_move_only_arg() {
    {
        const int expected = 99;
        int actual = 0;
        S<ts_policy, int(std::unique_ptr<int>)> sig;
        sig.connect_detached([&](auto x) {
            // std::cerr << "called with unique ptr-> address is "
            //           << static_cast<void*>(x.get()) << std::endl;
            return *x;
        });

        actual = sig.emit(std::make_unique<int>(expected));

        if (actual != expected) {
            return TEST_FAIL;
        }
    }

    {
        struct mystruct {
            int i = 0;
        };

        const int expected = 11;
        int actual = 0;
        S<ts_policy, int(mystruct)> sig;
        sig.connect_detached([=](struct mystruct x) {
            return x.i;
        });

        struct mystruct s;
        s.i = expected;
        actual = sig.emit(std::move(s));
        if (actual != expected) {
            return TEST_FAIL;
        }
    }

    return testStatus::TEST_PASS;
}

template<typename ts_policy>
enum testStatus test_disconnection() {
    // must throw an exception if connect() was called without
    // corresponding disconnect()
    // NOTE: we cannot catch exceptions thrown from DTORs, so the test
    // will crash in that case, but that is as expected.
    //{
    //    S<ts_policy, void()> sig;
    //    sig.connect([&]() {});
    //}

    // double-connect must throw.
    {
        S<ts_policy, void()> sig;

        bool exception_raised = false;

        try {
            sig.connect_detached([&]() {});
            sig.connect_detached([&]() {});
        } catch (...) {
            exception_raised = true;
        }

        if (!exception_raised) {
            return TEST_FAIL;
        }
    }

    // should be able to call disconnect repeatedly
    {
        S<ts_policy, void()> sig;

        auto conn = sig.connect([&]() {});
        if (!sig.connected()) {
            return TEST_FAIL;
        }

        conn->disconnect();
        if (sig.connected()) {
            return TEST_FAIL;
        }

        conn->disconnect();
        if (sig.connected()) {
            return TEST_FAIL;
        }

        conn->disconnect();
        if (sig.connected()) {
            return TEST_FAIL;
        }
    }

    // should be able to reconnect after disconnection.
    {
        S<ts_policy, void()> sig;

        bool exception_raised = false;

        try {
            auto conn = sig.connect([&]() {});
            if (!sig.connected()) {
                return TEST_FAIL;
            }

            conn->disconnect();
            if (sig.connected()) {
                return TEST_FAIL;
            }

            sig.connect_detached([&]() {});
            if (!sig.connected()) {
                return TEST_FAIL;
            }

        } catch (const std::exception &e) {
            std::cerr << "exception caught here: " << e.what() << std::endl;
            exception_raised = true;
        }

        if (exception_raised) {
            return TEST_FAIL;
        }
    }

    return testStatus::TEST_PASS;
}
#endif

}  // namespace monosignal

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

#if 1
    using TS = tarp::type_traits::thread_safe;
    using TU = tarp::type_traits::thread_unsafe;

    prepare_test_variables();

    /*==========================
     * Test 1:
     * check invoking an unconnected signal is ok (does not crash etc).
     */
    printf("TEST: [ts] unconnected signal invocation OK.\n");
    passed = run(monosignal::test_unconnected_invocation<TS>, TEST_PASS);
    update_test_counter(passed, test_unconnected_invocation);

    printf("TEST: [tu] unconnected signal invocation OK.\n");
    passed = run(monosignal::test_unconnected_invocation<TU>, TEST_PASS);
    update_test_counter(passed, test_unconnected_invocation);

    printf("TEST: [ts] connected signal returns correct value.\n");
    passed = run(monosignal::test_invocation<TS>, TEST_PASS);
    update_test_counter(passed, test_invocation);

    printf("TEST: [tu] connected signal returns correct value.\n");
    passed = run(monosignal::test_invocation<TU>, TEST_PASS);
    update_test_counter(passed, test_invocation);

    printf("TEST: [ts] test disconnection.\n");
    passed = run(monosignal::test_disconnection<TS>, TEST_PASS);
    update_test_counter(passed, test_disconnection);

    printf("TEST: [tu] test disconnection.\n");
    passed = run(monosignal::test_disconnection<TU>, TEST_PASS);
    update_test_counter(passed, test_disconnection);

    printf("TEST: [tu] test move-only arg.\n");
    passed = run(monosignal::test_move_only_arg<TU>, TEST_PASS);
    update_test_counter(passed, test_move_only_arg);

    report_test_summary();
#endif
}
