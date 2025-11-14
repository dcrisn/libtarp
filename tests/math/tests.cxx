#include <cmath>
#include <limits>
#include <tarp/math.hxx>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

using byte_vector = std::vector<std::uint8_t>;

TEST_CASE("sqrt") {
    using namespace tarp::math;
    REQUIRE(sqrt(0u) == 0);
    REQUIRE(sqrt(1u) == 1);
    REQUIRE(sqrt(2u) == 1);
    REQUIRE(sqrt(3u) == 1);
    REQUIRE(sqrt(4u) == 2);
    REQUIRE(sqrt(36u) == 6);
    REQUIRE(sqrt(81u) == 9);

    std::cerr << "--------\n";
    // check u64::max sqrt that we do not overflow
    auto x  = std::numeric_limits<std::uint64_t>::max();
    auto expected = 4294967295;
    REQUIRE(sqrt(x) == expected);

    // check that it compiles -- i.e. that we are constexpr.
    constexpr auto y = sqrt(1000u);
    (void)y;
}

int main(int argc, char **argv) {
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
