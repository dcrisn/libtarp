#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <cstring>
#include <stdexcept>

namespace tarp{


class ByteBuffer{
public:
    ByteBuffer(void) = default;
    ByteBuffer(const std::vector<uint8_t> &bytes);
    ByteBuffer(const uint8_t *bytes, std::size_t len);

    /*
     * NOTE: when copying or moving from a rhs ByteBuffer,
     * the offset position is copied as well! I.e. it
     * will not be reset to 0 (but this can be manually done) */
    ByteBuffer(ByteBuffer &&src);
    ByteBuffer &operator=(ByteBuffer &&rhs);

    ByteBuffer(const ByteBuffer &buff);
    ByteBuffer &operator=(const ByteBuffer &rhs);

    // TODO: add move constructor and assignment (make it work with std::move)
    // TODO: add copy constructor and assignment

    template <typename T>
    const T *get(bool advance = true);

    uint8_t get_byte(bool advance = true);

    template <typename T>
    std::shared_ptr<T> get_copy(bool advance = true);

    template <typename T>
    bool room4(void) const;

    template <typename T>
    void push(T *elem);

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
    std::size_t m_buff_offset = 0;
};

template <typename T>
bool ByteBuffer::room4(void) const{
    return (m_buff.size() - m_buff_offset >= sizeof(T));
}

template <typename T>
const T *ByteBuffer::get(bool advance){
    if (!room4<T>()) return nullptr;
    T *ret = static_cast<T*>(get_offset_ptr());

    if (advance) m_buff_offset += sizeof(T);

    return ret;
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
void ByteBuffer::push(T *elem){
    assert(elem);
    if (!elem){
        throw std::invalid_argument("insane attempt to push nullptr");
    }

    uint8_t *p = static_cast<uint8_t *>(elem);
    m_buff.insert(m_buff.end(), p, p+sizeof(T));
}

}  /* namespace tarp */
