#include <string>
#include <tarp/callback.hxx>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <vector>

using byte_vector = std::vector<std::uint8_t>;
using namespace tarp;

namespace {
// Helper functions for testing
int global_value = 0;

void static_func(void *, int val) {
    global_value = val;
}

int static_func_with_return(void *, int a, int b) {
    return a + b;
}

struct Counter {
    int count = 0;

    void increment(int amount) { count += amount; }

    int get_count() const { return count; }

    void reset() { count = 0; }

    int multiply(int a, int b) { return a * b; }
};

struct Logger {
    std::vector<std::string> logs;

    void log(const std::string &msg) { logs.push_back(msg); }

    std::string get_concatenated() const {
        std::string result;
        for (const auto &log : logs) {
            result += log;
        }
        return result;
    }
};
}  // namespace

TEST_CASE("Callback - default construction") {
    tarp::Callback<void(int)> cb;

    REQUIRE_FALSE(cb);
}

TEST_CASE("Callback - bind static function") {
    tarp::Callback<void(int)> cb;
    global_value = 0;

    cb.bind(nullptr, static_func);

    REQUIRE(cb);

    cb(42);
    REQUIRE(global_value == 42);

    cb(100);
    REQUIRE(global_value == 100);

    struct mystruct {
        std::string test;
    };

    mystruct s;
    cb.bind(&s, [](void *ptr, int x) {
        static_cast<mystruct *>(ptr)->test = std::to_string(x);
    });
    cb(99);
    REQUIRE(s.test == "99");
}

TEST_CASE("Callback - bind static function with return value") {
    tarp::Callback<int(int, int)> cb;

    cb.bind(nullptr, static_func_with_return);

    REQUIRE(cb);
    REQUIRE(cb(3, 4) == 7);
    REQUIRE(cb(10, 20) == 30);
    REQUIRE(cb(-5, 5) == 0);
}

TEST_CASE("Callback - bind member function") {
    Counter counter;
    tarp::Callback<void(int)> cb;

    cb.bind<&Counter::increment>(&counter);

    REQUIRE(cb);
    REQUIRE(counter.count == 0);

    cb(5);
    REQUIRE(counter.count == 5);

    cb(3);
    REQUIRE(counter.count == 8);
}

TEST_CASE("Callback - bind member function with return value") {
    Counter counter;
    tarp::Callback<int(int, int)> cb;

    cb.bind<&Counter::multiply>(&counter);

    REQUIRE(cb);
    REQUIRE(cb(3, 4) == 12);
    REQUIRE(cb(7, 8) == 56);
}

TEST_CASE("Callback - bind const member function") {
    Counter counter;
    counter.count = 42;

    tarp::Callback<int()> cb;
    cb.bind<&Counter::get_count>(&counter);

    REQUIRE(cb);
    REQUIRE(cb() == 42);

    counter.count = 100;
    REQUIRE(cb() == 100);
}

TEST_CASE("Callback - bind capturing lambda") {
    int captured = 10;
    tarp::Callback<int(int)> cb;

    cb.bind([captured](int x) {
        return x + captured;
    });

    REQUIRE(cb);
    REQUIRE(cb(5) == 15);
    REQUIRE(cb(20) == 30);
}

TEST_CASE("Callback - bind mutable capturing lambda") {
    int accumulator = 0;
    tarp::Callback<void(int)> cb;

    cb.bind([&accumulator](int val) {
        accumulator += val;
    });

    REQUIRE(cb);

    cb(10);
    REQUIRE(accumulator == 10);

    cb(5);
    REQUIRE(accumulator == 15);

    cb(-3);
    REQUIRE(accumulator == 12);
}

TEST_CASE("Callback - bind functor") {
    struct Multiplier {
        int factor;

        Multiplier(int f) : factor(f) {}

        int operator()(int x) const { return x * factor; }
    };

    tarp::Callback<int(int)> cb;
    cb.bind(Multiplier(5));

    REQUIRE(cb);
    REQUIRE(cb(3) == 15);
    REQUIRE(cb(10) == 50);
}

TEST_CASE("Callback - reset") {
    tarp::Callback<void(int)> cb;
    global_value = 0;

    cb.bind(nullptr, static_func);
    REQUIRE(cb);

    cb(42);
    REQUIRE(global_value == 42);

    cb.reset();
    REQUIRE_FALSE(cb);
}

TEST_CASE("Callback - reset with capturing lambda") {
    int captured = 100;
    tarp::Callback<int(int)> cb;

    cb.bind([captured](int x) {
        return x + captured;
    });
    REQUIRE(cb);
    REQUIRE(cb(5) == 105);

    cb.reset();
    REQUIRE_FALSE(cb);
}

TEST_CASE("rebind (calls reset() and) deallocates") {
    bool destructed = false;

    struct Multiplier {
        Multiplier(bool &flag) : m_flag(&flag) {}

        bool *m_flag = nullptr;
        int factor;

        Multiplier(int f, bool *flag) : m_flag(flag), factor(f) {}

        int operator()(int x) const { return x * factor; }

        ~Multiplier() {
            if (m_flag) *m_flag = true;
        }
    };

    struct dummy {
        int operator()(int) const { return 0; }
    };

    tarp::Callback<int(int)> cb;
    REQUIRE(!cb);
    REQUIRE(destructed == false);
    cb.bind(Multiplier(5, &destructed));
    REQUIRE(cb);
    REQUIRE(destructed == true);
    cb.bind(dummy());
    REQUIRE(destructed);
    REQUIRE(cb);
}

TEST_CASE("Callback - rebind") {
    Counter counter;
    tarp::Callback<void(int)> cb;

    cb.bind<&Counter::increment>(&counter);
    cb(10);
    REQUIRE(counter.count == 10);

    global_value = 0;
    cb.bind(nullptr, static_func);
    cb(99);
    REQUIRE(global_value == 99);
    REQUIRE(counter.count == 10);  // unchanged
}

TEST_CASE("Callback - copy constructor with function pointer") {
    Counter counter;
    tarp::Callback<void(int)> cb1;
    cb1.bind<&Counter::increment>(&counter);

    tarp::Callback<void(int)> cb2(cb1);

    REQUIRE(cb1);
    REQUIRE(cb2);

    cb2(5);
    REQUIRE(counter.count == 5);

    cb1(3);
    REQUIRE(counter.count == 8);
}

TEST_CASE("Callback - copy constructor with capturing lambda") {
    int value = 10;
    tarp::Callback<int(int)> cb1;
    cb1.bind([value](int x) {
        return x * value;
    });

    tarp::Callback<int(int)> cb2(cb1);

    REQUIRE(cb1);
    REQUIRE(cb2);

    REQUIRE(cb1(3) == 30);
    REQUIRE(cb2(3) == 30);
    REQUIRE(cb1(5) == 50);
    REQUIRE(cb2(5) == 50);
}

TEST_CASE("Callback - copy assignment with function pointer") {
    Counter counter;
    tarp::Callback<void(int)> cb1;
    cb1.bind<&Counter::increment>(&counter);

    tarp::Callback<void(int)> cb2;
    cb2 = cb1;

    REQUIRE(cb1);
    REQUIRE(cb2);

    cb2(7);
    REQUIRE(counter.count == 7);

    cb1(3);
    REQUIRE(counter.count == 10);
}

TEST_CASE("Callback - copy assignment with capturing lambda") {
    int multiplier = 4;
    tarp::Callback<int(int)> cb1;
    cb1.bind([multiplier](int x) {
        return x * multiplier;
    });

    tarp::Callback<int(int)> cb2;
    cb2 = cb1;

    REQUIRE(cb1);
    REQUIRE(cb2);

    REQUIRE(cb1(5) == 20);
    REQUIRE(cb2(5) == 20);
}

TEST_CASE("Callback - move constructor") {
    Counter counter;
    tarp::Callback<void(int)> cb1;
    cb1.bind<&Counter::increment>(&counter);

    tarp::Callback<void(int)> cb2(std::move(cb1));

    REQUIRE_FALSE(cb1);
    REQUIRE(cb2);

    cb2(15);
    REQUIRE(counter.count == 15);
}

TEST_CASE("Callback - move constructor with capturing lambda") {
    int value = 7;
    tarp::Callback<int(int)> cb1;
    cb1.bind([value](int x) {
        return x + value;
    });

    tarp::Callback<int(int)> cb2(std::move(cb1));

    REQUIRE_FALSE(cb1);
    REQUIRE(cb2);

    REQUIRE(cb2(3) == 10);
}

TEST_CASE("Callback - move assignment") {
    Counter counter;
    tarp::Callback<void(int)> cb1;
    cb1.bind<&Counter::increment>(&counter);

    tarp::Callback<void(int)> cb2;
    cb2 = std::move(cb1);

    REQUIRE_FALSE(cb1);
    REQUIRE(cb2);

    cb2(25);
    REQUIRE(counter.count == 25);
}

TEST_CASE("Callback - move assignment with capturing lambda") {
    int offset = 100;
    tarp::Callback<int(int)> cb1;
    cb1.bind([offset](int x) {
        return x + offset;
    });

    tarp::Callback<int(int)> cb2;
    cb2 = std::move(cb1);

    REQUIRE_FALSE(cb1);
    REQUIRE(cb2);

    REQUIRE(cb2(50) == 150);
}

TEST_CASE("Callback - self assignment") {
    Counter counter;
    tarp::Callback<void(int)> cb;
    cb.bind<&Counter::increment>(&counter);

    cb = cb;

    REQUIRE(cb);
    cb(10);
    REQUIRE(counter.count == 10);
}

TEST_CASE("Callback - void return type") {
    Logger logger;
    tarp::Callback<void(const std::string &)> cb;

    cb.bind<&Logger::log>(&logger);

    REQUIRE(cb);

    cb("Hello");
    cb("World");

    REQUIRE(logger.logs.size() == 2);
    REQUIRE(logger.logs[0] == "Hello");
    REQUIRE(logger.logs[1] == "World");
}

TEST_CASE("Callback - multiple parameters") {
    tarp::Callback<int(int, int, int)> cb;

    cb.bind([](int a, int b, int c) {
        return a + b * c;
    });

    REQUIRE(cb);
    REQUIRE(cb(1, 2, 3) == 7);
    REQUIRE(cb(10, 5, 2) == 20);
}

TEST_CASE("Callback - string parameters and return") {
    tarp::Callback<std::string(const std::string &, const std::string &)> cb;

    cb.bind([](const std::string &a, const std::string &b) {
        return a + " " + b;
    });

    REQUIRE(cb);
    REQUIRE(cb("Hello", "World") == "Hello World");
    REQUIRE(cb("foo", "bar") == "foo bar");
}

TEST_CASE("Callback - bind with context pointer") {
    struct Context {
        int multiplier = 5;
    };

    Context ctx;
    tarp::Callback<int(int)> cb;

    cb.bind(&ctx, [](void *ptr, int x) -> int {
        Context *c = static_cast<Context *>(ptr);
        return x * c->multiplier;
    });

    REQUIRE(cb);
    REQUIRE(cb(3) == 15);

    ctx.multiplier = 10;
    REQUIRE(cb(3) == 30);
}

TEST_CASE("Callback - chaining operations") {
    Counter counter;
    tarp::Callback<void(int)> cb1;
    tarp::Callback<void(int)> cb2;
    tarp::Callback<void(int)> cb3;

    cb1.bind<&Counter::increment>(&counter);
    cb2 = cb1;
    cb3 = std::move(cb2);

    REQUIRE(cb1);
    REQUIRE_FALSE(cb2);
    REQUIRE(cb3);

    cb1(5);
    REQUIRE(counter.count == 5);

    cb3(10);
    REQUIRE(counter.count == 15);
}

TEST_CASE("Callback - reassignment cleans up old state") {
    int captured1 = 10;
    int captured2 = 20;

    tarp::Callback<int(int)> cb;

    cb.bind([captured1](int x) {
        return x + captured1;
    });
    REQUIRE(cb(5) == 15);

    cb.bind([captured2](int x) {
        return x * captured2;
    });
    REQUIRE(cb(2) == 40);
}

TEST_CASE("Callback - stateless lambda (can decay to function pointer)") {
    tarp::Callback<int(int, int)> cb;

    // Stateless lambda - could be optimized by compiler
    cb.bind([](int a, int b) {
        return a - b;
    });

    REQUIRE(cb);
    REQUIRE(cb(10, 3) == 7);
    REQUIRE(cb(5, 8) == -3);
}

TEST_CASE("Callback - bool conversion") {
    tarp::Callback<void()> cb;

    REQUIRE_FALSE(static_cast<bool>(cb));

    cb.bind([]() {});
    REQUIRE(static_cast<bool>(cb));

    cb.reset();
    REQUIRE_FALSE(static_cast<bool>(cb));
}

TEST_CASE("Callback - different return types") {
    SUBCASE("bool") {
        tarp::Callback<bool(int)> cb;
        cb.bind([](int x) {
            return x > 0;
        });
        REQUIRE(cb(5) == true);
        REQUIRE(cb(-3) == false);
    }

    SUBCASE("double") {
        tarp::Callback<double(double, double)> cb;
        cb.bind([](double a, double b) {
            return a / b;
        });
        REQUIRE(cb(10.0, 2.0) == doctest::Approx(5.0));
    }

    SUBCASE("pointer") {
        int value = 42;
        tarp::Callback<int *()> cb;
        cb.bind([&value]() {
            return &value;
        });
        REQUIRE(*cb() == 42);
    }
}

TEST_CASE("Callback - no parameters") {
    int counter = 0;
    tarp::Callback<void()> cb;

    cb.bind([&counter]() {
        counter++;
    });

    REQUIRE(cb);

    cb();
    REQUIRE(counter == 1);

    cb();
    cb();
    REQUIRE(counter == 3);
}

TEST_CASE("Callback - reference parameters") {
    tarp::Callback<void(int &)> cb;

    cb.bind([](int &x) {
        x *= 2;
    });

    REQUIRE(cb);

    int value = 5;
    cb(value);
    REQUIRE(value == 10);

    cb(value);
    REQUIRE(value == 20);
}

int main(int argc, const char **argv) {
    doctest::Context ctx;

    ctx.setOption("abort-after",
                  1);  // default - stop after 5 failed asserts

    ctx.applyCommandLine(argc, argv);  // apply command line - argc / argv

    ctx.setOption("no-breaks",
                  true);  // override - don't break in the debugger

    int res = ctx.run();  // run test cases unless with --no-run

    if (ctx.shouldExit())  // query flags (and --exit) rely on this
    {
        return res;  // propagate the result of the tests
    }

    return 0;
}
