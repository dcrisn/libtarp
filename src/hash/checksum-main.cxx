#include <tarp/bits.hxx>
#include <tarp/cxxcommon.hxx>
#include <tarp/hash/checksum.hxx>
#include <tarp/hash/crc.hxx>
#include <tarp/string_utils.hxx>

// prevent collision with match from string_utils
#include <tarp/common.h>
#ifdef match
#undef match
#endif

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
namespace crc = tarp::hash::crc;
namespace checksum = tarp::hash::checksum;

namespace keys {
static inline constexpr auto fletcher16 = "fletcher16";
static inline constexpr auto fletcher32 = "fletcher32";
static inline constexpr auto fletcher64 = "fletcher64";
static inline constexpr auto crc8blt = "crc8-bluetooth";
static inline constexpr auto crc16_modbus = "crc16-modbus";
static inline constexpr auto crc16_kermit = "crc16-kermit";
static inline constexpr auto crc32_iso_hdlc = "crc32";
static inline constexpr auto crc32c = "crc32c";
static inline constexpr auto crc64go = "crc64go";
static inline constexpr auto crc64xz = "crc64xz";
};  // namespace keys

// print the data digest as a hexstring
template<typename T>
void print_checksum(T checksum) {
    std::cout << std::hex << std::showbase << std::setfill('0')
              << std::setw(tarp::bits::width_v<T> / 4) << checksum << std::endl;
}

// process data and print the crc computed from its contents.
template<typename T,
         typename context_t,
         T G,
         T rinit,
         T xor_out,
         bool reflect_in,
         bool reflect_out,
         typename fileread_func>
T print_crc_of_file(fileread_func &&process_file) {
    T checksum;
    context_t ctx;

    std::array<T, 256> t;

    crc::make_lookup_table(G, t);

    auto process_block = [&](auto *buffer, auto len) {
        checksum =
          crc::make_crc_1byte_aat<T, rinit, xor_out, reflect_in, reflect_out>(
            buffer, len, ctx, t);
    };

    auto on_finished = [](const auto *) {};
    process_file(process_block, on_finished);
    print_checksum(checksum);
    return checksum;
}

// process data and print the fletcher checksum computed from its contents.
template<typename T,
         typename context_t,
         typename fileread_func,
         typename data_block_processor_t,
         typename checksum_getter_t>
T print_fletcher_checksum_of_file(fileread_func &&process_file,
                                  data_block_processor_t &&data_block_processor,
                                  checksum_getter_t &&checksum_getter) {
    T checksum = 0;
    context_t ctx;
    std::uint32_t remaining = 0;
    std::uint32_t next_idx = 0;

    auto process_block = [&](auto *buffer, auto len) {
        next_idx = data_block_processor(ctx, buffer, len, false);
        remaining = len - next_idx;
    };

    auto on_finished = [&](auto *buffer) {
        if (remaining > 0) {
            data_block_processor(ctx, buffer + next_idx, remaining, true);
        }
        checksum = checksum_getter(ctx);
    };

    process_file(process_block, on_finished);
    print_checksum(checksum);
    return checksum;
}

#define do_print_crc_of_file(                              \
  T, namespace_to_use, context_type, func_to_process_file) \
    using namespace namespace_to_use;                      \
    print_crc_of_file<T,                                   \
                      context_type,                        \
                      G,                                   \
                      rinit,                               \
                      xor_out,                             \
                      reflect_in,                          \
                      reflect_out>(process_file);

void print_help([[maybe_unused]] const char **argv) {
    std::cerr << "Usage: " << argv[0] << " [--ALGO] FILE" << std::endl;

    std::cerr << "\n";

    std::cerr << "Print a digest of the contents of FILE produced by the "
                 "specified algorithm.\n";

    std::cerr << "ALGO must be one of the following (--crc32 is implied if "
                 "otherwise unspecified):\n";
    std::cerr << "   --fletcher16         16-bit fletcher checksum\n";
    std::cerr << "   --fletcher32         32-bit fletcher checksum\n";
    std::cerr << "   --fletcher64         64-bit fletcher checksum\n";
    std::cerr << "   --crc8-bluetooth     CRC-8/BLUETOOH\n";
    std::cerr << "   --crc16-modbus       CRC-16/MODBUS\n";
    std::cerr << "   --crc16-kermit       CRC-16/KERMIT\n";
    std::cerr << "   --crc32              CRC-32/ISO-HDLC\n";
    std::cerr << "   --crc32c             CRC-32/CASTAGNOLI\n";
    std::cerr << "   --crc64go            CRC-64/GO-ISO\n";
    std::cerr << "   --crc64xz            CRC-64/XZ\n";

    std::cerr << "\n";
    std::cerr << "The checksum will be brinted as a hexstring.\n";
    std::cerr
      << "For fletcher32 and fletcher64 the input is read as a sequence\n"
         "of u16s and u32s, respectively, in host byte order.\n";
}

int main(int argc, const char **argv) {
    using namespace tarp::utils;
    using namespace std::string_literals;

    if (argc < 2 || argc > 3) {
        print_help(argv);
        return -1;
    }

    std::string algo = "--crc32";  // default
    std::string fpath;

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == "-h"s or argv[i] == "--help"s) {
            print_help(argv);
            return 0;
        }
    }

    // if a single option, then it must be a file path.
    if (argc == 2) {
        if (str::starts_with(argv[1], "--", false)) {
            print_help(argv);
            return -1;
        }
        fpath = argv[1];
    } else if (argc == 3) {
        if (str::starts_with(argv[1], "--", false)) {
            algo = argv[1];
            fpath = argv[2];
        } else {
            algo = argv[2];
            fpath = argv[1];
        }
    }

    if (!str::starts_with(algo, "--", false)) {
        std::cerr << "Bad cli: no algorithm specified. Try --help.\n";
        return -1;
    }
    algo = string_utils::remove_prefix(algo, "--", false);

    if (!fs::is_regular_file(fpath)) {
        std::cerr << "No such file/not a regular file: " << fpath << std::endl;
        return -1;
    }

    std::ifstream f;
    try {
        f.open(fpath, std::ios_base::in | std::ios_base::binary);
    } catch (const std::exception &) {}

    if (!f.is_open()) {
        std::cerr << "Failed to open file " << fpath << std::endl;
        return -1;
    }

    static constexpr std::size_t BUFFSZ = 32768;
    std::vector<std::uint8_t> readbuff;
    readbuff.resize(BUFFSZ);
    char *p = reinterpret_cast<char *>(&readbuff[0]);
    const uint8_t *buff = reinterpret_cast<const uint8_t *>(p);

    // to be able to avoid code duplication here we use a number of
    // callbacks. This function itself is used as a callback that in turn
    // calls a callback to process each data block and then calls
    // one finalizer.
    auto process_file = [&f, p, &buff](auto &&process_block,
                                       auto &&on_finished) {
        while (true) {
            f.read(p, BUFFSZ);
            process_block(buff, f.gcount());
            if (f.eof() || f.fail()) {
                break;
            }
        }
        on_finished(buff);
    };

    if (algo == keys::fletcher16) {
        using namespace checksum;
        using namespace checksum::fletcher::fletcher16;
        print_fletcher_checksum_of_file<std::uint16_t, fletcher16_ctx>(
          process_file, process_block<false>, get_checksum);

    } else if (algo == keys::fletcher32) {
        using namespace checksum;
        using namespace checksum::fletcher::fletcher32;
        print_fletcher_checksum_of_file<std::uint32_t, fletcher32_ctx>(
          process_file, process_block<false>, get_checksum);

    } else if (algo == keys::fletcher64) {
        using namespace checksum;
        using namespace checksum::fletcher::fletcher64;
        print_fletcher_checksum_of_file<std::uint64_t, fletcher64_ctx>(
          process_file, process_block<false>, get_checksum);
    }

    else if (algo == keys::crc8blt) {
        do_print_crc_of_file(std::uint8_t,
                             crc::params::crc8_bluetooth,
                             crc::crc8_ctx,
                             process_file);
    }

    else if (algo == keys::crc16_kermit) {
        do_print_crc_of_file(std::uint16_t,
                             crc::params::crc16_kermit,
                             crc::crc16_ctx,
                             process_file);
    }

    else if (algo == keys::crc16_modbus) {
        do_print_crc_of_file(std::uint16_t,
                             crc::params::crc16_modbus,
                             crc::crc16_ctx,
                             process_file);
    }

    else if (algo == keys::crc32_iso_hdlc) {
        do_print_crc_of_file(
          std::uint32_t, crc::params::crc32_hdlc, crc::crc32_ctx, process_file);
    }

    else if (algo == keys::crc32c) {
        do_print_crc_of_file(
          std::uint32_t, crc::params::crc32c, crc::crc32_ctx, process_file);
    }

    else if (algo == keys::crc64go) {
        do_print_crc_of_file(
          std::uint64_t, crc::params::crc64_go, crc::crc64_ctx, process_file);
    }

    else if (algo == keys::crc64xz) {
        do_print_crc_of_file(
          std::uint64_t, crc::params::crc64_xz, crc::crc64_ctx, process_file);
    }

    else {
        std::cerr << "Unknown algorithm specified: '" << algo << "'\n";
        return -1;
    }

    return 0;
}
