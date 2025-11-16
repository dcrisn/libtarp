#include <tarp/word_iterator.hxx>
#include <tarp/random.hxx>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <vector>

using byte_vector = std::vector<std::uint8_t>;
namespace rd = tarp::random;
using namespace tarp;

TEST_CASE("word_iterator - basic iteration") {
    // Test with aligned buffer (4 bytes, u16 words)
    byte_vector buff = {0x01, 0x02, 0x03, 0x04};

    word_iterator<uint16_t> iter(buff.data(), buff.size(), false);

    uint16_t word;
    unsigned n;

    // First word
    n = iter.get_word(word);
    REQUIRE(n == 2);
    REQUIRE(word == 0x0201);  // Little endian host
    REQUIRE(iter.offset() == 2);
    REQUIRE(iter.remaining() == 2);

    // Second word
    n = iter.get_word(word);
    REQUIRE(n == 2);
    REQUIRE(word == 0x0403);
    REQUIRE(iter.offset() == 4);
    REQUIRE(iter.remaining() == 0);

    // End of iteration
    n = iter.get_word(word);
    REQUIRE(n == 0);
}

TEST_CASE("word_iterator - partial word at end") {
    byte_vector buff = {0x01, 0x02, 0x03};

    word_iterator<uint16_t> iter(buff.data(), buff.size(), false);

    uint16_t word;
    unsigned n;

    // First full word
    n = iter.get_word(word);
    REQUIRE(n == 2);
    REQUIRE(word == 0x0201);

    // Partial word (only 1 byte)
    n = iter.get_word(word);
    REQUIRE(n == 1);
    REQUIRE(word == 0x0003);  // Zero-padded
    REQUIRE(iter.offset() == 3);

    // End
    n = iter.get_word(word);
    REQUIRE(n == 0);
}

TEST_CASE("word_iterator - backtrack") {
    byte_vector buff = {0x01, 0x02, 0x03, 0x04, 0x05};

    word_iterator<uint16_t> iter(buff.data(), buff.size(), false);

    uint16_t word;

    // Read two words
    iter.get_word(word);
    REQUIRE(iter.offset() == 2);
    iter.get_word(word);
    REQUIRE(iter.offset() == 4);

    // Backtrack 2 bytes
    iter.backtrack(2);
    REQUIRE(iter.offset() == 2);
    REQUIRE(iter.remaining() == 3);

    // Read again
    unsigned n = iter.get_word(word);
    REQUIRE(n == 2);
    REQUIRE(word == 0x0403);

    // Backtrack more than offset (should go to 0)
    iter.backtrack(100);
    REQUIRE(iter.offset() == 0);
}

TEST_CASE("word_iterator - set_offset") {
    byte_vector buff = {0x01, 0x02, 0x03, 0x04};

    word_iterator<uint16_t> iter(buff.data(), buff.size(), false);

    uint16_t word;

    // Jump to offset 2
    iter.set_offset(2);
    REQUIRE(iter.offset() == 2);

    unsigned n = iter.get_word(word);
    REQUIRE(n == 2);
    REQUIRE(word == 0x0403);

    // Jump back to beginning
    iter.set_offset(0);
    n = iter.get_word(word);
    REQUIRE(word == 0x0201);

    // Try to set out of bounds
    REQUIRE_THROWS_AS(iter.set_offset(100), std::invalid_argument);
}

TEST_CASE("word_iterator - endianness swap u16") {
    byte_vector buff = {0x12, 0x34, 0x56, 0x78, 0xcc};

    // Without swap (little endian interpretation)
    {
        word_iterator<uint16_t> iter(buff.data(), buff.size(), false);
        uint16_t word;

        iter.get_word(word);
        REQUIRE(word == 0x3412);

        iter.get_word(word);
        REQUIRE(word == 0x7856);

        // partial, little endian
        REQUIRE(iter.get_word(word) == 1);
        REQUIRE(word == 0x00cc);
    }

    // With swap (simulate big endian buffer)
    {
        word_iterator<uint16_t> iter(buff.data(), buff.size(), true);
        uint16_t word;

        iter.get_word(word);
        REQUIRE(word == 0x1234);  // Swapped

        iter.get_word(word);
        REQUIRE(word == 0x5678);  // Swapped

        // partial, big endian
        REQUIRE(iter.get_word(word) == 1);
        REQUIRE(word == 0xcc00);
    }
}

TEST_CASE("word_iterator - endianness swap u32") {
    byte_vector buff = {0x12, 0x34, 0x56, 0x78};

    // Without swap
    {
        word_iterator<uint32_t> iter(buff.data(), buff.size(), false);
        uint32_t word;

        iter.get_word(word);
        REQUIRE(word == 0x78563412);
    }

    // With swap
    {
        word_iterator<uint32_t> iter(buff.data(), buff.size(), true);
        uint32_t word;

        iter.get_word(word);
        REQUIRE(word == 0x12345678);
    }
}

TEST_CASE("word_iterator - empty buffer") {
    byte_vector buff;

    word_iterator<uint16_t> iter(buff.data(), buff.size(), false);

    uint16_t word;
    unsigned n = iter.get_word(word);

    REQUIRE(n == 0);
    REQUIRE(iter.offset() == 0);
    REQUIRE(iter.remaining() == 0);
}

TEST_CASE("word_iterator - single byte") {
    byte_vector buff = {0x42};

    word_iterator<uint16_t> iter(buff.data(), buff.size(), false);

    uint16_t word;
    unsigned n = iter.get_word(word);

    REQUIRE(n == 1);
    REQUIRE(word == 0x0042);  // Zero-padded
    REQUIRE(iter.remaining() == 0);
}

TEST_CASE("word_iterator - different word sizes") {
    byte_vector buff = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    // u8
    {
        word_iterator<uint8_t> iter(buff.data(), buff.size(), false);
        uint8_t word;

        for (size_t i = 0; i < 8; ++i) {
            unsigned n = iter.get_word(word);
            REQUIRE(n == 1);
            REQUIRE(word == buff[i]);
        }
    }

    // u16
    {
        word_iterator<uint16_t> iter(buff.data(), buff.size(), false);
        uint16_t word;

        unsigned n = iter.get_word(word);
        REQUIRE(n == 2);
        REQUIRE(word == 0x0201);

        n = iter.get_word(word);
        REQUIRE(n == 2);
        REQUIRE(word == 0x0403);
    }

    // u32
    {
        word_iterator<uint32_t> iter(buff.data(), buff.size(), false);
        uint32_t word;

        unsigned n = iter.get_word(word);
        REQUIRE(n == 4);
        REQUIRE(word == 0x04030201);

        n = iter.get_word(word);
        REQUIRE(n == 4);
        REQUIRE(word == 0x08070605);
    }

    // u64
    {
        word_iterator<uint64_t> iter(buff.data(), buff.size(), false);
        uint64_t word;

        unsigned n = iter.get_word(word);
        REQUIRE(n == 8);
        REQUIRE(word == 0x0807060504030201ULL);
    }
}

TEST_CASE("word_iterator - partial word with swap") {
    byte_vector buff = {0x12, 0x34, 0x56};

    word_iterator<uint32_t> iter(buff.data(), buff.size(), true);

    uint32_t word;
    unsigned n = iter.get_word(word);

    REQUIRE(n == 3);
    // Only 3 bytes: {0x12, 0x34, 0x56}, with 0x0 implicitly
    // padded at the end; then after swap:
    REQUIRE(word == 0x12345600);
    
    // 0x12345600
}

TEST_CASE("word_iterator - accessor methods") {
    byte_vector buff = {0x01, 0x02, 0x03, 0x04};

    word_iterator<uint16_t> iter(buff.data(), buff.size(), false);

    REQUIRE(iter.buff() == buff.data());
    REQUIRE(iter.buffsz() == 4);
    REQUIRE(iter.offset() == 0);
    REQUIRE(iter.remaining() == 4);

    uint16_t word;
    iter.get_word(word);

    REQUIRE(iter.offset() == 2);
    REQUIRE(iter.remaining() == 2);
}

TEST_CASE("word_iterator - fuzz test") {
    constexpr size_t N = 100;

    for (size_t i = 0; i < N; ++i) {
        // Random buffer size
        const size_t buffsz = rd::randsz(1, 1000); 
        const auto buff = rd::bytes(buffsz);

        // Test with u16
        {
            word_iterator<uint16_t> iter(buff.data(), buff.size(), false);

            size_t bytes_consumed = 0;
            uint16_t word;
            unsigned n;

            while ((n = iter.get_word(word)) > 0) {
                bytes_consumed += n;
                REQUIRE(n <= sizeof(uint16_t));
            }

            REQUIRE(bytes_consumed == buffsz);
            REQUIRE(iter.offset() == buffsz);
            REQUIRE(iter.remaining() == 0);
        }

        // Test with u32
        {
            word_iterator<uint32_t> iter(buff.data(), buff.size(), false);

            size_t bytes_consumed = 0;
            uint32_t word;
            unsigned n;

            while ((n = iter.get_word(word)) > 0) {
                bytes_consumed += n;
                REQUIRE(n <= sizeof(uint32_t));
            }

            REQUIRE(bytes_consumed == buffsz);
            REQUIRE(iter.offset() == buffsz);
            REQUIRE(iter.remaining() == 0);
        }
    }
}

TEST_CASE("word_iterator - backtrack and re-read") {
    byte_vector buff = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

    word_iterator<uint16_t> iter(buff.data(), buff.size(), false);

    uint16_t word;

    // Read first word
    iter.get_word(word);
    uint16_t first = word;
    REQUIRE(first == 0xBBAA);

    // Backtrack and read again
    iter.backtrack(2);
    iter.get_word(word);
    REQUIRE(word == first);  // Should get same value

    // Continue forward
    iter.get_word(word);
    REQUIRE(word == 0xDDCC);

    // Partial word at end
    unsigned n = iter.get_word(word);
    REQUIRE(n == 1);
    REQUIRE(word == 0x00EE);

    // Backtrack the partial and verify we can re-read it
    iter.backtrack(1);
    n = iter.get_word(word);
    REQUIRE(n == 1);
    REQUIRE(word == 0x00EE);
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
