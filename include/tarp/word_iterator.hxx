#pragma once

#include <tarp/bits.hxx>

#include <algorithm>  // For std::reverse_copy if needed for manual swap
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace tarp {

// Used to consume _words_ from a buffer and perform
// any endianness byte-swapping adjustments.
// Avoids cluttering e.g. checksum functions with
// inline endianness checks and byte-swapping logic.
template<typename Word>
class word_iterator final {
    static_assert(std::is_unsigned_v<Word>);

public:
    explicit word_iterator(const std::uint8_t *data,
                           std::size_t len,
                           bool swap_endian)
        : m_data(data), m_len(len), m_swap(swap_endian), m_offset(0) {}

    word_iterator() = delete;

    // Consume a word from the buffer.
    // Return the number of bytes consumed [always <= sizeof(word)].
    //
    // If 0 is returned, then no bytes were consumed: iteration
    // is over.
    //
    // Else if < sizeof(word) is returned, then fewer than
    // sizeof(word) were available, but these were consumed anyway
    // and formed into a word. If this is undesirable, the
    // called can backtrack by calling backtrack() or set_offset().
    //
    // Else the return value is sizeof(word), meaning a whole
    // word was processed.
    unsigned get_word(Word &out) {
        static_assert(std::is_unsigned_v<Word>);

        if (m_offset < m_len) {
            const std::size_t remaining = m_len - m_offset;
            const std::size_t n =
              remaining > m_word_size ? m_word_size : remaining;

            // Read bytes (zero-pad if needed)
            out = 0;
            std::memcpy(&out, m_data + m_offset, n);
            m_offset += n;

            // Swap if needed
            if (m_swap) {
                out = bits::byteswap(out);
            }

            return n;
        }

        return 0;
    }

    // current offset in the associated buffer
    [[nodiscard]] std::size_t offset() const { return m_offset; }

    // bytes remaining to be consumed
    [[nodiscard]] std::size_t remaining() const { return m_len - m_offset; }

    // go forward, backward, etc
    void set_offset(std::size_t offset) {
        if (offset > m_len) {
            throw std::invalid_argument("Attempt to set out-of-bounds offset");
        }
        m_offset = offset;
    }

    // Set the offset n positions back
    void backtrack(std::size_t n) { m_offset -= std::min(n, m_offset); }

    // pointer to the first byte of the associated buffer
    [[nodiscard]] const std::uint8_t *buff() const { return m_data; }

    // length in bytes of the associated buffer
    [[nodiscard]] std::size_t buffsz() const { return m_len; }

private:
    const uint8_t *m_data = nullptr;
    const std::size_t m_len = 0;
    static inline constexpr std::size_t m_word_size = sizeof(Word);
    const bool m_swap = false;
    std::size_t m_offset = 0;
};

}  // namespace tarp
