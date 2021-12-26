#include "stream_reassembler.hh"

#include <sstream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in
// `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _buffer(capacity, ' ')
    , _start(0)
    , _start_index(0)
    , _unassembled_bytes(0)
    , _eof(false)
    , _filled(nullptr) {
    _filled.reset(new bool[_capacity]);
    for (uint32_t i = 0; i < _capacity; i++)
        _filled[i] = false;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    uint64_t from = 0;
    if (index < _start_index) {
        from = _start_index - index;
    }

    uint64_t size = std::min(data.size(), _capacity - _output.buffer_size() - index + _start_index);
    for (uint64_t i = from; i < size; i++) {
        auto j = (i + _start + index - _start_index) % _capacity;
        _buffer[j] = data[i];
        _unassembled_bytes += !_filled[j];
        _filled[j] = true;
    }
    uint64_t i = 0;
    std::stringstream ss;
    while (true) {
        auto j = (_start + i) % _capacity;
        if (!_filled[j]) {
            break;
        }
        _filled[j] = false;
        _unassembled_bytes--;
        ss << _buffer[j];
        i++;
    }
    _start = (_start + i) % _capacity;
    _start_index = _start_index + i;

    _output.write(ss.str());

    _eof |= (eof && size == data.size());
    if (_eof && unassembled_bytes() == 0) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0 && _output.buffer_empty(); }
