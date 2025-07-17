#include <tarp/hash/crc.hxx>

#include <cstdint>
#include <cstring>

namespace tarp {
namespace hash {
namespace crc {

namespace bitaat {

// This macro generates two function overloads, one for processing data in
// blocks, taking a ctx as argument, and one to process all data upfront,
// that dispatches to the other overload. The following macro invocation
//    make_functions(crc64_xz, std::uint64_t, crc64_ctx, crc64_xz);
// generates the following function definitions:
#if 0
std::uint64_t crc64_xz(const uint8_t *msg, std::size_t len, crc64_ctx &ctx) {
    using namespace params::crc64_xz;
    return make_crc_1bit_aat<std::uint64_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

std::uint64_t crc64_xz(const uint8_t *msg, std::size_t len) {
    crc64_ctx ctx;
    return crc64_xz(msg, len, ctx);
}
#endif
#define make_functions(                                                   \
  function_name, return_type, context_type, params_namespace)             \
    return_type function_name(                                            \
      const std::uint8_t *msg, std::size_t len, context_type &ctx) {      \
        using namespace params::params_namespace;                         \
        return make_crc_1bit_aat<return_type,                             \
                                 rinit,                                   \
                                 xor_out,                                 \
                                 reflect_in,                              \
                                 reflect_out>(G, msg, len, ctx);          \
    }                                                                     \
                                                                          \
    return_type function_name(const std::uint8_t *msg, std::size_t len) { \
        context_type ctx;                                                 \
        return function_name(msg, len, ctx);                              \
    }

make_functions(crc16_dectx, std::uint16_t, crc16_ctx, crc16_dectx);
make_functions(crc16_usb, std::uint16_t, crc16_ctx, crc16_usb);
make_functions(crc16_gsm, std::uint16_t, crc16_ctx, crc16_gsm);
make_functions(crc16_kermit, std::uint16_t, crc16_ctx, crc16_kermit);
make_functions(crc16_modbus, std::uint16_t, crc16_ctx, crc16_modbus);
make_functions(crc8_bluetooth, std::uint8_t, crc8_ctx, crc8_bluetooth);
make_functions(crc32_bzip2, std::uint32_t, crc32_ctx, crc32_bzip2);
make_functions(crc32_cksum, std::uint32_t, crc32_ctx, crc32_cksum);
make_functions(crc32c, std::uint32_t, crc32_ctx, crc32c);
make_functions(crc32_iso_hdlc, std::uint32_t, crc32_ctx, crc32_hdlc);
make_functions(crc64_go, std::uint64_t, crc64_ctx, crc64_go);
make_functions(crc64_xz, std::uint64_t, crc64_ctx, crc64_xz);
#undef make_functions
}  // namespace bitaat

namespace byteaat {
#define make_functions(                                                   \
  function_name, return_type, context_type, params_namespace)             \
    return_type function_name(const std::uint8_t *msg,                    \
                              std::size_t len,                            \
                              context_type &ctx,                          \
                              const lookup_table_t<return_type> &table) { \
        using namespace params::params_namespace;                         \
        return make_crc_1byte_aat<return_type,                            \
                                  rinit,                                  \
                                  xor_out,                                \
                                  reflect_in,                             \
                                  reflect_out>(msg, len, ctx, table);     \
    }                                                                     \
                                                                          \
    return_type function_name(const std::uint8_t *msg,                    \
                              std::size_t len,                            \
                              const lookup_table_t<return_type> &table) { \
        context_type ctx;                                                 \
        return function_name(msg, len, ctx, table);                       \
    }


make_functions(crc16_gsm, std::uint16_t, crc16_ctx, crc16_gsm);
make_functions(crc32c, std::uint32_t, crc32_ctx, crc32c);
make_functions(crc64_xz, std::uint64_t, crc64_ctx, crc64_xz);
#undef make_functions
}  // namespace byteaat


}  // namespace crc
}  // namespace hash
}  // namespace tarp
