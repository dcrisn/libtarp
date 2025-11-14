#include <tarp/hash/checksum/fletcher.hxx>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <endian.h>

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

using byte_vector = std::vector<std::uint8_t>;

// implementations taken from
// https://en.wikipedia.org/wiki/Fletcher%27s_checksum
// in order to be able to test my implementations;
// I can only assume the wikipedia code is correct;
// finding reference test vectors for this checksum
// appears impossible beyond the basic (and very very short)
// strings in the wikipedia article.
namespace reference {
uint16_t fletcher16(const uint8_t *data, size_t len) {
    uint32_t c0, c1;

    /*  Found by solving for c1 overflow: */
    /* n > 0 and n * (n+1) / 2 * (2^8-1) < (2^32-1). */
    for (c0 = c1 = 0; len > 0;) {
        size_t blocklen = len;
        if (blocklen > 5802) {
            blocklen = 5802;
        }
        len -= blocklen;
        do {
            const auto byte1 = *data++;
            c0 += byte1;
            // c0 = c0 + *data++;
            c1 = c1 + c0;
            std::cerr << "Added byte1=" << static_cast<unsigned>(byte1)
                      << " c0=" << c0 << ", c1=" << c1 << std::endl;
        } while (--blocklen);
        c0 = c0 % 255;
        c1 = c1 % 255;
    }
    return (c1 << 8 | c0);
}

uint32_t fletcher32(const uint16_t *data, size_t len) {
    uint32_t c0, c1;
    len = (len + 1) & ~1; /* Round up len to words */

    /* We similarly solve for n > 0 and n * (n+1) / 2 * (2^16-1) < (2^32-1)
     * here. */
    /* On modern computers, using a 64-bit c0/c1 could allow a group size of
     * 23726746. */
    for (c0 = c1 = 0; len > 0;) {
        size_t blocklen = len;
        if (blocklen > 360 * 2) {
            blocklen = 360 * 2;
        }
        len -= blocklen;
        do {
            c0 = c0 + *data++;
            c1 = c1 + c0;
        } while ((blocklen -= 2));
        c0 = c0 % 65535;
        c1 = c1 % 65535;
    }
    return (c1 << 16 | c0);
}

uint32_t fletcher64(const uint16_t *data, size_t len) {
    uint32_t c0, c1;
    len = (len + 1) & ~1; /* Round up len to words */

    /* We similarly solve for n > 0 and n * (n+1) / 2 * (2^16-1) < (2^32-1)
     * here. */
    /* On modern computers, using a 64-bit c0/c1 could allow a group size of
     * 23726746. */
    for (c0 = c1 = 0; len > 0;) {
        size_t blocklen = len;
        if (blocklen > 360 * 2) {
            blocklen = 360 * 2;
        }
        len -= blocklen;
        do {
            c0 = c0 + *data++;
            c1 = c1 + c0;
        } while ((blocklen -= 2));
        c0 = c0 % 92681;
        c1 = c1 % 92681;
    }
    return (c1 << 16 | c0);
}

};  // namespace reference

template<typename cb_t, typename T>
std::pair<bool, T> test(cb_t f, T expected_result, const std::string &input) {
    std::vector<std::uint8_t> bytes;
    for (auto c : input) {
        bytes.push_back(c);
    }

    auto res = f(bytes);
    return {(res == expected_result), res};
}

using namespace tarp::hash::checksum;

TEST_CASE("basics") {
    std::uint64_t NUM_TESTS = 0;
    std::uint64_t NUM_PASSED = 0;

    // Columns: host-byte-order input string, network-byte-order input string,
    // expected result. The expected result is the same in both cases but
    // when handling the network byte order input, endianness conversion must be
    // performed by the algorithm (which is the point of the test).

    // These test input strings are taken from wikipedia
    // (https://en.wikipedia.org/wiki/Fletcher%27s_checksum)

    // endianness does not matter here since every block is 1 single byte.
    std::vector<std::tuple<std::string, std::uint16_t>> fletcher16_tests {
      {"abcde",    0xC8F0},
      {"abcdef",   0x2057},
      {"abcdefgh", 0x0627},
    };

    std::vector<std::tuple<std::string, std::uint32_t>> fletcher32_tests {
      {"abcde",    0xF04FC729},
      {"abcdef",   0x56502D2A},
      {"abcdefgh", 0xEBE19591},
    };

    std::vector<std::tuple<std::string, std::uint64_t>> fletcher64_tests {
      {"abcde",    0xC8C6C527646362C6},
      {"abcdef",   0xC8C72B276463C8C6},
      {"abcdefgh", 0x312E2B28CCCAC8C6},
    };

    auto print_passed_or_not =
      [](bool passed, const auto &input, auto expected, auto actual) {
          std::cerr << "[" << (passed ? "x" : " ") << "]" << " : " << input
                    << ". Results: expected=" << expected
                    << ", actual=" << actual << std::endl;
      };

    std::cerr << "Fletcher16 tests: \n";
    for (const auto &[input, expected_result] : fletcher16_tests) {
        auto [passed, res] = test(
          [](auto in) {
              return fletcher16<false>(&in[0], in.size());
          },
          expected_result,
          input);

        NUM_TESTS++;
        if (passed) NUM_PASSED++;

        print_passed_or_not(passed, input, expected_result, res);
    }

    std::cerr << "Fletcher32 tests: \n";
    for (const auto &[input, expected_result] : fletcher32_tests) {
        {
            auto [passed, res] = test(
              [](auto in) {
                  return fletcher32<false>(&in[0], in.size());
              },
              expected_result,
              input);

            NUM_TESTS++;
            if (passed) NUM_PASSED++;

            print_passed_or_not(passed, false, expected_result, res);
        }
    }

    std::cerr << "Fletcher64 tests: \n";
    for (const auto &[input, expected_result] : fletcher64_tests) {
        {
            auto [passed, res] = test(
              [](auto in) {
                  return fletcher64<false>(&in[0], in.size());
              },
              expected_result,
              input);

            NUM_TESTS++;
            if (passed) NUM_PASSED++;

            print_passed_or_not(passed, false, expected_result, res);
        }
    }

    std::cerr << "\n"
              << "PASSED: " << NUM_PASSED << "/" << NUM_TESTS << std::endl;
    // REQUIRE(NUM_PASSED == NUM_TESTS);
}

TEST_CASE("basics") {
    // REQUIRE(fletcher16_ctx::modulus ==
    // std::numeric_limits<std::uint32_t>::max());

    {
        byte_vector input = {'a', 'b', 'c', 'd', 'e'};
        fletcher16_ctx::checksum_t expected = 51440;
        fletcher16_ctx ctx;
        auto actual =
          tarp::hash::checksum::fletcher16<false>(input.data(), input.size());

        REQUIRE(reference::fletcher16(input.data(), input.size()) == expected);
        REQUIRE(actual == expected);
    }

    std::cerr << "-------------\n";
    {
        byte_vector input = {'b', 'a', 'd', 'c', 'e'};
        fletcher32_ctx::checksum_t expected = 0xF04FC729;
        fletcher32_ctx ctx;
        auto actual =
          tarp::hash::checksum::fletcher32<true>(input.data(), input.size());

        // REQUIRE(
        //   reference::fletcher32(reinterpret_cast<std::uint16_t
        //   *>(input.data()),
        //                         input.size()) == expected);
        // REQUIRE(actual == expected);
    }
    std::cerr << "-------------\n";
    {
        byte_vector input = {'a', 'b', 'c', 'd', 'e', 0};
        fletcher32_ctx::checksum_t expected = 0xF04FC729;
        fletcher32_ctx ctx;
        auto actual =
          tarp::hash::checksum::fletcher32<false>(input.data(), input.size());

        REQUIRE(
          reference::fletcher32(reinterpret_cast<std::uint16_t *>(input.data()),
                                input.size()) == expected);
        REQUIRE(actual == expected);
    }
    std::cerr << "-------------\n";
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
