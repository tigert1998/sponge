#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _buffer(capacity, ' ') {
    _len = 0;
    _start = 0;
    _capacity = capacity;
}

size_t ByteStream::write(const string &data) {
    uint64_t ret = std::min(data.size(), remaining_capacity());
    for (uint64_t i = 0; i < ret; i++) {
        _buffer[(i + _start + _len) % _capacity] = data[i];
    }
    _len += ret;
    _bytes_written += ret;
    return ret;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::string ret(len, ' ');
    for (uint64_t i = 0; i < len; i++) {
        ret[i] = _buffer[(_start + i) % _capacity];
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    _start = (_start + len) % _capacity;
    _bytes_read += len;
    _len -= len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto s = peek_output(len);
    pop_output(len);
    return s;
}

void ByteStream::end_input() { _input_ended = true; }

size_t ByteStream::buffer_size() const { return _len; }

bool ByteStream::buffer_empty() const { return _len == 0; }

bool ByteStream::eof() const { return _input_ended && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _len; }
