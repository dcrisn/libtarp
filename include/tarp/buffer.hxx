#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

#include <cstring>

namespace std {

namespace tarp {

/*
 */
class ByteBuffer {
public:
    ByteBuffer(void) = default;
    ByteBuffer(const std::vector<std::byte> &bytes);
    ByteBuffer(const std::byte *bytes, std::size_t len);
    ByteBuffer(int fd, std::int32_t num_bytes = -1);

    /*
     * NOTE: when copying or moving from a rhs ByteBuffer,
     * the offset position is copied as well! I.e. it
     * will not be reset to 0 (but this can be manually done) */
    ByteBuffer(ByteBuffer &&src);
    ByteBuffer &operator=(ByteBuffer &&rhs);

    ByteBuffer(const ByteBuffer &buff);
    ByteBuffer &operator=(const ByteBuffer &rhs);

    /* Get a T if there are enough bytes */
    template<typename T>
    std::optional<T> get(bool advance = true) const;

    /* Get a unique_ptr to a T if there are enough bytes, else null. */
    template<typename T>
    std::unique_ptr<T> get_ptr(bool advance = true);

    std::byte get_byte(bool advance = true);

    template<typename T>
    bool room4(void) const;

    template<typename T>
    void push(const T &elem);

    void push(const std::vector<std::byte> &v);
    void push(const std::byte *start, size_t len);

    void from_fd(int fd, std::int32_t num_bytes = -1);

    void skip(size_t nbytes);
    void unwind(size_t nbytes);
    void truncate(void);
    void reset(void);
    void clear(void);
    std::size_t size(void) const;
    std::size_t left(void) const;

private:
    const void *get_offset_ptr(void) const;
    bool within_bounds(void) const;
    void swap(ByteBuffer &other);

    std::vector<std::byte> m_buff;
    mutable std::size_t m_buff_offset = 0;
};

template<typename T>
bool ByteBuffer::room4(void) const {
    return (m_buff.size() - m_buff_offset >= sizeof(T));
}

/*
 * NOTE: type punning via memcpy is well-defined in C++
 * under certain circumstances (trivally copiable and
 * constructible types etc. In this category fall C-compatible
 * POD types which are of course in fact primarily of interest).
 * Starting with C++20, this is formally specified in the standard
 * (refer to proposal p0593r6).
 * NOTE: reinterpret_cast, as usual, is mostly UB; only use it to cast
 * an arbitrary type to unsigned char or std::byte.
 */
template<typename T>
std::optional<T> ByteBuffer::get(bool advance) const {
    if (!room4<T>()) return {};

    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_constructible_v<T>);
    T ret;
    memcpy(&ret, get_offset_ptr(), sizeof(T));

    if (advance) m_buff_offset += sizeof(T);
    return ret;
}

template<typename T>
std::unique_ptr<T> ByteBuffer::get_ptr(bool advance) {
    if (!room4<T>()) return nullptr;

    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_constructible_v<T>);
    auto ret = std::make_unique<T>();
    memcpy(ret.get(), get_offset_ptr(), sizeof(T));

    if (advance) m_buff_offset += sizeof(T);

    return ret;
}

template<typename T>
void ByteBuffer::push(const T &elem) {
    // NOTE: this is well defined; anything can be cast to std::byte, and
    // unsigned char.
    const std::byte *p = reinterpret_cast<const std::byte *>(&elem);
    m_buff.insert(m_buff.end(), p, p + sizeof(T));
}

} /* namespace tarp */
