#include <tarp/hash/checksum/inet.hxx>

namespace tarp {
namespace hash {
namespace checksum {
namespace inet {
void update_checksum(inetcksum_ctx &ctx,
                     const std::uint8_t *buff,
                     std::size_t bufflen,
                     std::size_t change_offset,
                     std::size_t change_len,
                     const std::uint8_t *change) {
    // no change
    if (change_len == 0) {
        return;
    }

    if (change_offset + change_len > bufflen) {
        throw std::invalid_argument("change starts or ends out of bounds");
    }

    constexpr std::size_t wordsz = sizeof(std::uint16_t);
    std::uint8_t word[wordsz];

    std::size_t len = change_len;

    // if the change starts at an odd offset, we must pair
    // with the previous byte in the original buffer
    // to get a word that's correctly aligned.
    if (change_offset & 0x1) {
        std::uint16_t oldval, newval;
        std::memcpy(word, buff + change_offset - 1, wordsz);
        std::memcpy(&oldval, word, wordsz);

        std::memcpy(&word[0], buff + change_offset - 1, sizeof(std::uint8_t));
        std::memcpy(&word[1], change, sizeof(std::uint8_t));
        std::memcpy(&newval, word, wordsz);

        len--;

        // implicit promotion means adding ~oldval
        // will actually have us add maxu32 if we do
        // not cast!
        ctx.sum += static_cast<std::uint16_t>(~oldval);
        ctx.sum += static_cast<std::uint16_t>(newval);

        // std::cerr << "added oldval=" <<
        // int_to_hexstring(std::uint16_t(~oldval))
        //           << " newval=" << int_to_hexstring(newval)
        //           << "; sum=" << int_to_hexstring(ctx.sum) << std::endl;
    }

    const std::uint8_t *old = buff + change_offset + (change_len - len);
    const std::uint8_t *p = change + (change_len - len);

    const std::size_t n = len >> 1;
    for (std::size_t i = 0; i < n; ++i) {
        std::uint16_t oldval, newval;
        std::memcpy(&oldval, old, wordsz);
        std::memcpy(&newval, p, wordsz);

        old += wordsz;
        p += wordsz;

        // subtract old contribution from sum;
        // add new contribution to sum;
        // see RFC1624-4.
        ctx.sum += static_cast<std::uint16_t>(~oldval);
        ctx.sum += static_cast<std::uint16_t>(newval);

        // std::cerr << "added oldval=" <<
        // int_to_hexstring(std::uint16_t(~oldval))
        //           << " newval=" << int_to_hexstring(newval)
        //           << "; sum=" << int_to_hexstring(ctx.sum) << std::endl;
    }

    // if the length is not even and we have not reached
    // the end of the original buffer, then:
    // - we must pair with the next byte to get correct word alignment
    // OR
    // - we must handle trailing byte
    if ((len & 0x1)) {
        std::uint16_t oldval = 0, newval = 0;

        // final trailing byte, cannot pair, add directly.
        // and we must replace the byte in the 'joint' array!
        if (change_offset + change_len == bufflen) {
            std::memcpy(&oldval,
                        buff + change_offset + change_len - 1,
                        sizeof(std::uint8_t));

            const std::uint8_t *ptr = change + change_len - 1;
            ctx.joint[0] = *ptr;
            std::memcpy(&newval, ptr, sizeof(std::uint8_t));
        }
        // pair with the next byte in the original buffer
        else {
            std::memcpy(word, buff + change_offset + change_len - 1, wordsz);
            std::memcpy(&oldval, word, wordsz);

            std::memcpy(
              &word[0], change + change_len - 1, sizeof(std::uint8_t));
            std::memcpy(&word[1],
                        buff + change_offset + change_len,
                        sizeof(std::uint8_t));
            std::memcpy(&newval, word, wordsz);
        }

        ctx.sum += static_cast<std::uint16_t>(~oldval);
        ctx.sum += static_cast<std::uint16_t>(newval);

        // std::cerr << "added oldval=" <<
        // int_to_hexstring(std::uint16_t(~oldval))
        //           << " newval=" << int_to_hexstring(newval)
        //           << "; sum=" << int_to_hexstring(ctx.sum) << std::endl;
    }

    // now call get_checksum() as usual to get the final checksum
}
}  // namespace inet
}  // namespace checksum
}  // namespace hash
}  // namespace tarp
