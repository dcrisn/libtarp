#include <tarp/buffer.hxx>

using namespace tarp;
using namespace std;

ByteBuffer::ByteBuffer(const uint8_t *bytes, size_t len){
    if (!bytes) return;
    m_buff.insert(m_buff.end(), bytes, bytes+len);
}

ByteBuffer::ByteBuffer(const vector<uint8_t> &bytes)
    : m_buff(bytes)
{
}

/*
 * For the copy-and-swap idiom */
void ByteBuffer::swap(ByteBuffer &other){
    // swap the vector pointers...
    std::swap(m_buff, other.m_buff);
    std::swap(m_buff_offset, other.m_buff_offset);
}

// copy constructor
ByteBuffer::ByteBuffer(const ByteBuffer &src){
    m_buff.insert(m_buff.end(), src.m_buff.begin(), src.m_buff.end());
}

// copy assignment operator
ByteBuffer &ByteBuffer::operator=(const ByteBuffer &rhs){
    if (&rhs != this) {
        ByteBuffer tmp(rhs);  /* copy-construct from rhs */
        tmp.swap(*this);
    }
    return *this;
}

// move constructor
ByteBuffer::ByteBuffer(ByteBuffer &&src){
    src.swap(*this);
}

// move assignment operator
ByteBuffer &ByteBuffer::operator=(ByteBuffer &&rhs){
    ByteBuffer tmp(std::move(rhs)); /* move-construct from rhs */
    tmp.swap(*this);
    return *this;
}

/*
 * NOTE: this always returns a pointer, even if it's out
 * of bounds. An out-of-bounds check must be performed
 * by a higher-level wrapper (see get<>) */
const void *ByteBuffer::get_offset_ptr(void) const{
    return &m_buff.at(m_buff_offset);
}

bool ByteBuffer::within_bounds(void) const {
    return m_buff_offset < m_buff.size();
}

uint8_t ByteBuffer::get_byte(bool advance){
    if (!within_bounds()) return 0;
    uint8_t byte = m_buff.at(m_buff_offset);
    if (advance) m_buff_offset++;
    return byte;
}

void ByteBuffer::truncate(void){
    if (!within_bounds()) return;

    m_buff.erase(m_buff.begin()+m_buff_offset, m_buff.end());
}

size_t ByteBuffer::size(void) const {
    return m_buff.size();
}

size_t ByteBuffer::left(void) const{
    return m_buff.size() - m_buff_offset;
}

void ByteBuffer::skip(size_t nbytes){
    m_buff_offset += nbytes;
    m_buff_offset = std::min(m_buff_offset, m_buff.size());
}

void ByteBuffer::unwind(size_t nbytes){
    if (nbytes > m_buff_offset){
        m_buff_offset = 0;
        return;
    }

    m_buff_offset -= nbytes;
}

void ByteBuffer::reset(void){
    m_buff_offset = 0;
}

void ByteBuffer::clear(void){
    m_buff_offset = 0;
    m_buff.clear();
}


