#include <tarp/random.hxx>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <cmath>
#include <stdexcept>


using namespace tarp::random;

TEST_CASE("Linear Generator Basic Functionality") {
    // Start at 0.5, decrease by 0.1 each step, limit 0.1
    pgen::linear linear_gen(0.5f, -0.1f, 0.1f);

    // Initial call (Step 0)
    REQUIRE(doctest::Approx(linear_gen.p()) == 0.5f);
    // After 1 advance (Step 1): 0.5 - 0.1 = 0.4
    REQUIRE(doctest::Approx(linear_gen.p()) == 0.4f);
    // After 2 advances (Step 2): 0.4 - 0.1 = 0.3
    REQUIRE(doctest::Approx(linear_gen.p()) == 0.3f);
    // After 3 advances (Step 3): 0.3 - 0.1 = 0.2
    REQUIRE(doctest::Approx(linear_gen.p()) == 0.2f);

    linear_gen.p();
    for (unsigned i = 0; i < 10; ++i) {
        REQUIRE(doctest::Approx(linear_gen.p()) == 0.1f);
    }
}

TEST_CASE("Linear Generator Advance=False Behvaior") {
    pgen::linear linear_gen(0.5f, -0.1f, 0.0f);

    // First call advances to Step 1
    REQUIRE(doctest::Approx(linear_gen.p(true)) == 0.5f);
    // Next probability (at Step 1) is 0.4.
    // Call p(false) to peek at Step 1 value without advancing
    REQUIRE(doctest::Approx(linear_gen.p(false)) == 0.4f);
    // Call p(false) again, it should still be 0.4 because we didn't advance
    REQUIRE(doctest::Approx(linear_gen.p(false)) == 0.4f);
    // Call p(true) to use the 0.4 probability and advance to Step 2
    REQUIRE(doctest::Approx(linear_gen.p(true)) == 0.4f);
    // The next probability (at Step 2) is now 0.3
    REQUIRE(doctest::Approx(linear_gen.p(false)) == 0.3f);
}

TEST_CASE("Linear Generator Limit and Clamping") {
    // increase rapidly towards a limit of 0.8
    pgen::linear gen_increase(0.5f, 0.2f, 0.8f);

    REQUIRE(doctest::Approx(gen_increase.p()) == 0.5f);  // 0.5
    REQUIRE(doctest::Approx(gen_increase.p()) == 0.7f);  // 0.7
    REQUIRE(doctest::Approx(gen_increase.p()) ==
            0.8f);  // 0.9 clamped to 0.8 (limit)
    REQUIRE(doctest::Approx(gen_increase.p()) ==
            0.8f);  // 1.1 clamped to 0.8 (limit)

    // Decrease rapidly towards a limit of 0.2
    pgen::linear gen_decrease(0.9f, -0.4f, 0.2f);
    REQUIRE(doctest::Approx(gen_decrease.p()) == 0.9f);  // 0.9
    REQUIRE(doctest::Approx(gen_decrease.p()) == 0.5f);  // 0.5
    REQUIRE(doctest::Approx(gen_decrease.p()) ==
            0.2f);  // 0.1 clamped to 0.2 (limit)
    REQUIRE(doctest::Approx(gen_decrease.p()) ==
            0.2f);  // -0.3 clamped to 0.2 (limit)
}

TEST_CASE("Linear Generator Invalid Arguments") {
    REQUIRE_THROWS_AS(pgen::linear(-0.1f, 0.1f, 0.5f), std::invalid_argument);
    REQUIRE_THROWS_AS(pgen::linear(0.5f, 0.1f, -0.1f), std::invalid_argument);
}

TEST_CASE("Exponential Generator Basic Functionality - Decay") {
    // Start at 0.8, decay by factor of 0.5 (halve each time)
    pgen::exponential exp_gen(0.8f, 0.5f, 0.0f);

    REQUIRE(doctest::Approx(exp_gen.p()) == 0.8f);  // Step 0: 0.8 * 0.5^0 = 0.8
    REQUIRE(doctest::Approx(exp_gen.p()) == 0.4f);  // Step 1: 0.8 * 0.5^1 = 0.4
    REQUIRE(doctest::Approx(exp_gen.p()) == 0.2f);  // Step 2: 0.8 * 0.5^2 = 0.2
    REQUIRE(doctest::Approx(exp_gen.p()) == 0.1f);  // Step 3: 0.8 * 0.5^3 = 0.1
}

TEST_CASE("Exponential Generator Basic Functionality - Growth") {
    // Start at 0.1, grow by factor of 2 (double each time)
    pgen::exponential exp_gen(0.1f, 2.0f, 1.0f);

    REQUIRE(doctest::Approx(exp_gen.p()) == 0.1f);  // Step 0: 0.1 * 2^0 = 0.1
    REQUIRE(doctest::Approx(exp_gen.p()) == 0.2f);  // Step 1: 0.1 * 2^1 = 0.2
    REQUIRE(doctest::Approx(exp_gen.p()) == 0.4f);  // Step 2: 0.1 * 2^2 = 0.4
    REQUIRE(doctest::Approx(exp_gen.p()) == 0.8f);  // Step 3: 0.1 * 2^3 = 0.8
    REQUIRE(doctest::Approx(exp_gen.p()) ==
            1.0f);  // Step 4: 0.1 * 2^4 = 1.6, clamped to 1.0
}

TEST_CASE("Exponential Generator Limit and Clamping") {
    // Decrease rapidly towards a limit of 0.3
    pgen::exponential gen_decrease(0.9f, 0.3f, 0.3f);
    REQUIRE(doctest::Approx(gen_decrease.p()) == 0.9f);
    REQUIRE(doctest::Approx(gen_decrease.p()) ==
            0.3f);  // 0.9*0.3=0.27, clamped to 0.3 (limit)
    REQUIRE(doctest::Approx(gen_decrease.p()) == 0.3f);  // stays at limit
}

TEST_CASE("Exponential Generator Invalid Arguments") {
    REQUIRE_THROWS_AS(pgen::exponential(0.5f, 0.0f, 0.5f),
                      std::invalid_argument);  // Base cannot be 0
    REQUIRE_THROWS_AS(pgen::exponential(0.5f, -1.0f, 0.5f),
                      std::invalid_argument);  // Base cannot be negative
    REQUIRE_THROWS_AS(pgen::exponential(0.5f, 2.0f, -0.1f),
                      std::invalid_argument);  // Limit cannot be negative
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
