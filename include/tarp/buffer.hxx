#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <cassert>

namespace tarp{

/*
 * (1) This gives back a raw writable pointer through which calling
 * code can modify the underlying bytes in the buffer. NOTE this
 * relies on the user writing within bounds. It's therefore unsafe
 * and should be avoided wherever possible. NOTE the pointer returned
 * is only valid while nothing else is pushed or removed from the buffer.
 * If any other operation is invoked on the buffer, a new pointer must
 * be obtained, since the memory location of the underlying bytes
 * might have changed!
 */
class ByteBuffer{
public:
    ByteBuffer(void) = default;
    ByteBuffer(const std::vector<uint8_t> &bytes);
    ByteBuffer(const uint8_t *bytes, std::size_t len);
    ByteBuffer(int fd, ssize_t num_bytes = -1);

    /*
     * NOTE: when copying or moving from a rhs ByteBuffer,
     * the offset position is copied as well! I.e. it
     * will not be reset to 0 (but this can be manually done) */
    ByteBuffer(ByteBuffer &&src);
    ByteBuffer &operator=(ByteBuffer &&rhs);

    ByteBuffer(const ByteBuffer &buff);
    ByteBuffer &operator=(const ByteBuffer &rhs);

    template <typename T>
    const T *get(bool advance = true) const;

    /* (1) */
    template <typename T>
    T *get_writable(bool advance = true);

    uint8_t get_byte(bool advance = true);

    template <typename T>
    std::shared_ptr<T> get_copy(bool advance = true);

    template <typename T>
    bool room4(void) const;

    template <typename T>
    void push(const T &elem);

    void push(const std::vector<uint8_t> &v);
    void push(const uint8_t *start, size_t len);

    void from_fd(int fd, size_t num_bytes = -1);

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

    std::vector<std::uint8_t> m_buff;
    mutable std::size_t m_buff_offset = 0;
};

template <typename T>
bool ByteBuffer::room4(void) const{
    return (m_buff.size() - m_buff_offset >= sizeof(T));
}

template <typename T>
const T *ByteBuffer::get(bool advance) const{
    if (!room4<T>()) return nullptr;
    const T *ret = static_cast<const T *>(get_offset_ptr());

    if (advance) m_buff_offset += sizeof(T);

    return ret;
}

template <typename T>
T *ByteBuffer::get_writable(bool advance) {
    auto const_this = const_cast<const ByteBuffer *>(this);
    const T *ptr = const_this->get<T>();
    return const_cast<T*>(ptr);
}

template <typename T>
std::shared_ptr<T> ByteBuffer::get_copy(bool advance){
    if (!room4<T>()) return nullptr;
    T *ret = new T;
    std::memcpy(ret, get_offset_ptr(), sizeof(T));
    if (advance) m_buff_offset += sizeof(T);
    return ret;
}

template <typename T>
void ByteBuffer::push(const T &elem){
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&elem);
    m_buff.insert(m_buff.end(), p, p+sizeof(T));
}

}  /* namespace tarp */
