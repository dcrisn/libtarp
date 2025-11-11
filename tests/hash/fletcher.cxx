#include <endian.h>
#include <tarp/hash/checksum/fletcher.hxx>

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

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

int main(int, const char **) {
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

    std::vector<std::tuple<std::string, std::string, std::uint32_t>>
      fletcher32_tests {
        {"abcde",    "badce",    0xF04FC729},
        {"abcdef",   "badcfe",   0x56502D2A},
        {"abcdefgh", "badcfehg", 0xEBE19591},
    };

    std::vector<std::tuple<std::string, std::string, std::uint64_t>>
      fletcher64_tests {
        {"abcde",    "dcbae",    0xC8C6C527646362C6},
        {"abcdef",   "dcbafe",   0xC8C72B276463C8C6},
        {"abcdefgh", "dcbahgfe", 0x312E2B28CCCAC8C6},
    };

    auto print_passed_or_not =
      [](bool passed, bool nbo, const auto &input, auto expected, auto actual) {
          std::cerr << "[" << (passed ? "x" : " ") << "]"
                    << (nbo ? " ~ NBO ~" : " ~ HBO ~") << " : " << input
                    << ". Results: expected=" << expected
                    << ", actual=" << actual << std::endl;
      };

#if 1
    std::cerr << "Fletcher16 tests: \n";
    for (const auto &[input, expected_result] : fletcher16_tests) {
        auto [passed, res] = test(
          [](auto in) {
              return fletcher16<false>(&in[0], in.size());
          },
          expected_result,
          input);

        print_passed_or_not(passed, false, input, expected_result, res);
    }
#endif

    std::cerr << "Fletcher32 tests: \n";
    for (const auto &[host_byte_order_input,
                      network_byte_order_input,
                      expected_result] : fletcher32_tests) {
        {
            auto [passed, res] = test(
              [](auto in) {
                  return fletcher32<false>(&in[0], in.size());
              },
              expected_result,
              host_byte_order_input);
            print_passed_or_not(
              passed, false, host_byte_order_input, expected_result, res);
        }

        {
            auto [passed, res] = test(
              [](auto in) {
                  return fletcher32<true>(&in[0], in.size());
              },
              expected_result,
              network_byte_order_input);
            print_passed_or_not(
              passed, true, network_byte_order_input, expected_result, res);
        }
    }

#if 1
    std::cerr << "Fletcher64 tests: \n";
    for (const auto &[host_byte_order_input,
                      network_byte_order_input,
                      expected_result] : fletcher64_tests) {
        {
            auto [passed, res] = test(
              [](auto in) {
                  return fletcher64<false>(&in[0], in.size());
              },
              expected_result,
              host_byte_order_input);

            print_passed_or_not(
              passed, false, host_byte_order_input, expected_result, res);
        }

        {
            auto [passed, res] = test(
              [](auto in) {
                  return fletcher64<true>(&in[0], in.size());
              },
              expected_result,
              network_byte_order_input);

            print_passed_or_not(
              passed, true, network_byte_order_input, expected_result, res);
        }
    }
#endif

    return 0;
}
