#include "stream_reassembler.hh"

#include <sstream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _buffer(capacity, ' '),
      _start(0), _start_index(0), _assembled(0)
{
    _filled.reset(new bool[_capacity]);
    for (uint32_t i = 0; i < _capacity; i++)
        _filled[i] = false;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof)
{
    uint32_t size = std::min(data.size(), _capacity - index + _start_index);
    for (uint32_t i = 0; i < size; i++)
    {
        _buffer[(i + _start) % _capacity] = data[i];
        _filled[(i + _start) % _capacity] = true;
    }
    std::stringstream ss;
    while (_filled[_assembled])
    {
        ss << _buffer[_assembled];
        _assembled = (_assembled + 1) % _capacity;
    }
    ss.str();
}

size_t StreamReassembler::unassembled_bytes() const { return {}; }

bool StreamReassembler::empty() const { return {}; }
