#include <tarp/hash/crc.hxx>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <cstdint>

namespace crc = tarp::hash::crc;

// generate some fast byte-at-a-time CRC. Verify they give the same results as
// the bit-at-a-time analogues.
namespace byte_aat {
uint16_t crc16_modbus(const uint8_t *msg, size_t len) {
    crc::crc16_ctx ctx;
    std::array<std::uint16_t, 256> t;

    using namespace crc::params::crc16_modbus;
    crc::make_lookup_table<std::uint16_t>(G, t);

    return crc::make_crc_1byte_aat<std::uint16_t,
                                   rinit,
                                   xor_out,
                                   reflect_in,
                                   reflect_out>(msg, len, ctx, t);
}

std::uint8_t crc8_bluetooth(const uint8_t *msg, std::size_t len) {
    crc::crc8_ctx ctx;
    std::array<std::uint8_t, 256> t;

    using namespace crc::params::crc8_bluetooth;
    crc::make_lookup_table<std::uint8_t>(G, t);

    return crc::
      make_crc_1byte_aat<std::uint8_t, rinit, xor_out, reflect_in, reflect_out>(
        msg, len, ctx, t);
}

std::uint32_t crc32c(const uint8_t *msg, std::size_t len) {
    crc::crc32_ctx ctx;
    std::array<std::uint32_t, 256> t;

    using namespace crc::params::crc32c;
    crc::make_lookup_table<std::uint32_t>(G, t);
    return crc::make_crc_1byte_aat<std::uint32_t,
                                   rinit,
                                   xor_out,
                                   reflect_in,
                                   reflect_out>(msg, len, ctx, t);
}
}  // namespace byte_aat

namespace keys {
static inline constexpr auto crc16_modbus = "crc16_modbus";
static inline constexpr auto crc16_modbus_byteaat = "crc16_modbus_byteaat";
static inline constexpr auto crc16_usb = "crc16_usb";
static inline constexpr auto crc16_kermit = "crc16_kermit";
static inline constexpr auto crc16_gsm = "crc16_gsm";
static inline constexpr auto crc16_dectx = "crc16_dectx";
static inline constexpr auto crc8_bluetooth = "crc8_bluetooth";
static inline constexpr auto crc8_bluetooth_byteaat = "crc8_bluetooth_byteaat";
static inline constexpr auto crc32_bzip2 = "crc32_bzip2";
static inline constexpr auto crc32c = "crc32c";
static inline constexpr auto crc32c_byteaat = "crc32c_byteaat";
static inline constexpr auto crc32_cksum = "crc32_cksum";
static inline constexpr auto crc32_iso_hdlc = "crc32_iso_hdlc";
static inline constexpr auto crc64_go = "crc64_go";
static inline constexpr auto crc64_xz = "crc64_xz";
};  // namespace keys

namespace bitaat = tarp::hash::crc::bitaat;

int main(int, const char **) {
    uint8_t arr[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

    printf("sizeof array is %zu\n", sizeof(arr));

    std::vector<std::vector<std::uint8_t>> test_inputs {
      {},

      {0x0},

      {0x1},

      {0xff, 0x00},

      {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39},

      {0x32,
       0x36, 0x34,
       0x39, 0x31,
       0x36, 0x34,
       0x38, 0x39,
       0x31, 0x31,
       0x31, 0x30,
       0x30, 0x31,
       0x30},
    };

    std::map<std::string, std::vector<std::optional<std::uint64_t>>>
      expected_outputs {
        {keys::crc8_bluetooth, {0x00, 0x00, 0x6B, 0xA6, 0x26, 0x58}            },

        {keys::crc16_modbus,   {0xFFFF, 0x40BF, 0x807E, 0x4040, 0x4B37, 0x6D5A}},

        {keys::crc16_usb,      {0x0000, 0xBF40, 0x7F81, 0xBFBF, 0xB4C8, 0x92A5}},

        {keys::crc16_gsm,      {0xFFFF, 0xFFFF, 0xEFDE, 0xFC00, 0xCE3C, 0x5E71}},

        {keys::crc16_kermit,   {0x0000, 0x0000, 0x1189, 0xFFC0, 0x2189, 0xCA4A}},

        {keys::crc16_dectx,    {0x0000, 0x0000, 0x0589, 0x81D4, 0x007f, 0x234E}},

        {keys::crc32_bzip2,
         {0x00000000,
          0xB1F7404B,
          0xB5365DFC,
          0xB1F7BF4B,
          0xFC891918,
          0x7D0AB23B}                                                          },


        {keys::crc32c,
         {0x00000000, 0x527D5351, 0xA016D052, 0x52825351, 0xE3069283, 0xE2A2E787

         }                                                                     },


        {keys::crc32_cksum,
         {0xFFFFFFFF, 0xFFFFFFFF, 0xFB3EE248, 0xB140DB36, 0x765E7680, 0x282790F3

         }                                                                     },


        {keys::crc32_iso_hdlc,
         {0x00000000, 0xD202EF8D, 0xA505DF1B, 0xD2FDEF8D, 0xCBF43926, 0x0E5F4083

         }                                                                     },

        // https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat-bits.64
        {keys::crc64_go,
         {{}, {}, {}, {}, 0xb90956c775a41001, {}

         }                                                                     },

        // https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat-bits.64
        {keys::crc64_xz,
         {{}, {}, {}, {}, 0x995dc9bbdf1939fa, {}

         }                                                                     }
    };

    expected_outputs[keys::crc8_bluetooth_byteaat] =
      expected_outputs[keys::crc8_bluetooth];

    expected_outputs[keys::crc16_modbus_byteaat] =
      expected_outputs[keys::crc16_modbus];

    expected_outputs[keys::crc32c_byteaat] = expected_outputs[keys::crc32c];

    std::vector<std::tuple<
      std::string,
      std::function<std::uint64_t(const std::uint8_t *msg, std::size_t len)>>>
      testers {
        {keys::crc8_bluetooth,
         [](auto m, auto l) {
             return bitaat::crc8_bluetooth(m, l);
         }},

        {keys::crc8_bluetooth_byteaat,
         [](auto m, auto l) {
             return byte_aat::crc8_bluetooth(m, l);
         }},

        {keys::crc16_modbus,
         [](auto m, auto l) {
             return bitaat::crc16_modbus(m, l);
         }},

        {keys::crc16_modbus_byteaat,
         [](auto m, auto l) {
             return byte_aat::crc16_modbus(m, l);
         }},

        {keys::crc16_dectx,
         [](auto m, auto l) {
             return bitaat::crc16_dectx(m, l);
         }},

        {keys::crc16_usb,
         [](auto m, auto l) {
             return bitaat::crc16_usb(m, l);
         }},

        {keys::crc16_gsm,
         [](auto m, auto l) {
             return bitaat::crc16_gsm(m, l);
         }},

        {keys::crc16_kermit,
         [](auto m, auto l) {
             return bitaat::crc16_kermit(m, l);
         }},

        {keys::crc32_bzip2,
         [](auto m, auto l) {
             return bitaat::crc32_bzip2(m, l);
         }},

        {keys::crc32c,
         [](auto m, auto l) {
             return bitaat::crc32c(m, l);
         }},

        {keys::crc32c_byteaat,
         [](auto m, auto l) {
             return byte_aat::crc32c(m, l);
         }},

        {keys::crc32_cksum,
         [](auto m, auto l) {
             return bitaat::crc32_cksum(m, l);
         }},

        {keys::crc32_iso_hdlc,
         [](auto m, auto l) {
             return bitaat::crc32_iso_hdlc(m, l);
         }},

        {keys::crc64_go,
         [](auto m, auto l) {
             return bitaat::crc64_go(m, l);
         }},

        {keys::crc64_xz,
         [](auto m, auto l) {
             return bitaat::crc64_xz(m, l);
         }},
    };

    for (const auto &[tester_name, run_test] : testers) {
        std::cerr << "\n";
        std::cerr << "========== " << tester_name
                  << " =============" << std::endl;

        const auto &expected = expected_outputs[tester_name];

        if (expected.empty()) {
            std::cerr << " ~ Skipped, no tests.\n";
            continue;
        }

        if (expected.size() != test_inputs.size()) {
            std::cerr << " ~ Skipped, bad tests.\n";
            continue;
        }

        unsigned n = 0;
        for (const auto &input : test_inputs) {
            auto result = run_test(input.data(), input.size());
            if (!expected[n].has_value()) {
                std::cerr << "[x] " << "expected==?? actual==" << std::hex
                          << std::showbase << result << std::endl;
            } else if (result == *expected[n]) {
                std::cerr << "[x] " << "expected==actual==" << std::hex
                          << std::showbase << result << std::endl;
            } else {
                std::cerr << "[ ] " << "expected==" << std::hex << std::showbase
                          << *expected[n] << " != " << "actual==" << std::hex
                          << std::showbase << result << std::endl;
            }
            ++n;
        }
    }
}
