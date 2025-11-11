#include <cstdio>
#include <random>
#include <tarp/hash/checksum.hxx>
#include <tarp/ioutils.hxx>
#include <tarp/string_utils.hxx>

#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace str = tarp::utils::str;
namespace io = tarp::utils::io;
namespace cksum = tarp::hash::checksum;

// define in inet_checksum_inputs.cxx
extern std::vector<std::pair<std::string, std::string>> inet_checksum_inputs;

// Function taken from RFC1071-4.1, with the only modifications beying
// syntactical and replacing the c-cast with reinterpret_cast.
// This can be used for validating other implementations.
std::uint16_t inet_checksum_rfc(const std::uint8_t *buff, std::size_t len) {
    std::uint32_t sum = 0;

    while (len > 1) {
        /*  This is the inner loop */
        const std::uint16_t v = *reinterpret_cast<const std::uint16_t *>(buff);
        sum += v;
        buff += 2;
        len -= 2;
    }

    /*  Add left-over byte, if any */
    if (len > 0) {
        const std::uint8_t v = *reinterpret_cast<const std::uint8_t *>(buff);
        sum += v;
    }

    /*  Fold 32-bit sum to 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return ~sum;
}

#define REQUIRE(exp)                                                   \
    do {                                                               \
        if (!(exp)) {                                                  \
            fprintf(stderr, "%s FAILED at line %d\n", #exp, __LINE__); \
            exit(1);                                                   \
        }                                                              \
    } while (0)

void print_result(bool check_passed,
                  bool hbo,
                  bool blocked,
                  std::size_t blocksz,
                  std::uint16_t expected,
                  std::uint16_t actual,
                  const std::vector<std::uint8_t> &input,
                  unsigned check_counter,
                  std::size_t &pass_counter) {
    const std::string BO = hbo ? "HBO" : "NBO";
    const std::string STATUS = check_passed ? "PASSED" : "FAILED!";

    std::stringstream ss;

    ss << "CHECK " << check_counter << "[" << BO;
    if (blocked) {
        ss << ", blocksz=" << blocksz;
    }
    ss << "] ";
    ss << "with input of " << input.size() << " bytes " << STATUS;
    if (check_passed) {
        pass_counter++;
        ss << " (output=" << actual << ")" << std::endl;
    } else {
        ss << " (output=" << actual << ", expected=" << expected << ")"
           << std::endl;
    }

    std::cerr << ss.str() << std::endl;
}

std::uint16_t process_in_blocks(const std::vector<std::uint8_t> &bytes,
                                std::size_t blocksz) {
    cksum::inetcksum_ctx ctx;
    std::size_t len = bytes.size();
    std::size_t offset = 0;

    // must still invoke once! otherwise a checksum that is all 0s
    // will get us a result of all 0s, when in fact it should be ~0!
    // so we have to go through the function still.
    while (len >= blocksz) {
        //std::cerr << "processing block of size " << blocksz << " at offset "
        //          << offset << std::endl;
        cksum::inet::update_checksum(bytes.data() + offset, blocksz, ctx);
        len -= blocksz;
        offset += blocksz;
    }

    // if len not a multiple of the blocksz, we have one pass left.
    if (len > 0) {
        cksum::inet::update_checksum(bytes.data() + offset, len, ctx);
        //std::cerr << "processing block of size " << len << " at offset "
        //          << offset << std::endl;
    }

    return cksum::inet::get_checksum(ctx);
}

int main(int, const char **) {
    // one for nbo, one for hbo, both in blocks and upfront
    std::size_t checks_made = inet_checksum_inputs.size() * 4;
    std::size_t checks_passed = 0;

    std::random_device rd;
    std::mt19937 rng(rd());

    for (std::size_t i = 0; i < inet_checksum_inputs.size(); ++i) {
        std::cerr << "\n\n";
        std::cerr << " === TEST: " << i << " ===" << std::endl;
        const auto &[hbo_input_str, nbo_input_str] = inet_checksum_inputs[i];

        std::string e;
        const auto hbo_input = str::hexstring_to_bytes(hbo_input_str, e);
        REQUIRE(e.empty());
        const auto nbo_input = str::hexstring_to_bytes(nbo_input_str, e);
        REQUIRE(e.empty());

        // allow for bigger blocks than the checksum itself, to test
        // that possibility as well.
        std::uniform_int_distribution<std::size_t> pick_blocksz(
          1, hbo_input.size() + 10);

        // --------- HBO

        std::cerr << "Computing HBO reference result...\n";
        const auto hbo_input_reference_result =
          inet_checksum_rfc(hbo_input.data(), hbo_input.size());

        std::cerr << "Computing HBO one-shot result...\n";
        const auto hbo_input_result =
          cksum::inetv4(hbo_input.data(), hbo_input.size());

        std::cerr << "Computing HBO blocked result...\n";
        const bool hbo_input_result_valid =
          hbo_input_result == hbo_input_reference_result;

        const auto blocksz = pick_blocksz(rng);
        const auto hbo_input_result_blocked =
          process_in_blocks(hbo_input, blocksz);
        const bool hbo_input_result_blocked_valid =
          hbo_input_result_blocked == hbo_input_reference_result;

        print_result(hbo_input_result_valid,
                     true,
                     false,
                     0,
                     hbo_input_reference_result,
                     hbo_input_result,
                     hbo_input,
                     i,
                     checks_passed);

        print_result(hbo_input_result_blocked_valid,
                     true,
                     true,
                     blocksz,
                     hbo_input_reference_result,
                     hbo_input_result_blocked,
                     hbo_input,
                     i,
                     checks_passed);


        // --------- NBO

        std::cerr << "Computing NBO reference result...\n";
        const auto nbo_input_reference_result =
          inet_checksum_rfc(nbo_input.data(), nbo_input.size());

        std::cerr << "Computing NBO one-shot result...\n";
        const auto nbo_input_result =
          cksum::inetv4(nbo_input.data(), nbo_input.size());

        const bool nbo_input_result_valid =
          nbo_input_result == nbo_input_reference_result;

        std::cerr << "Computing NBO blocked result...\n";
        const auto nbo_input_result_blocked =
          process_in_blocks(nbo_input, blocksz);
        const bool nbo_input_result_blocked_valid =
          nbo_input_result_blocked == nbo_input_reference_result;

        print_result(nbo_input_result_valid,
                     false,
                     false,
                     0,
                     nbo_input_reference_result,
                     nbo_input_result,
                     nbo_input,
                     i,
                     checks_passed);

        print_result(nbo_input_result_blocked_valid,
                     false,
                     true,
                     blocksz,
                     nbo_input_reference_result,
                     nbo_input_result_blocked,
                     nbo_input,
                     i,
                     checks_passed);
    }

    std::cerr << "Passed: " << checks_passed << "/" << checks_made << std::endl;

    return 0;
}
