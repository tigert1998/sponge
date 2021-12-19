#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto seqno = seg.header().seqno;
    if (_isn == nullptr) {
        if (seg.header().syn) {
            _isn.reset(new WrappingInt32(seqno));
            _reassembler.push_substring(std::string(seg.payload().str()), 0, seg.header().fin);
        }
    } else {
        uint64_t abs_seqno = unwrap(seqno, *_isn, _reassembler.stream_out().bytes_written() + 1);
        uint64_t stream_index;
        std::string data = std::string(seg.payload().str());
        if (abs_seqno == 0) {
            data = data.substr(1);
            stream_index = 0;
        } else {
            stream_index = abs_seqno - 1;
        }
        _reassembler.push_substring(data, stream_index, seg.header().fin);
    }
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_isn == nullptr) {
        return std::nullopt;
    } else {
        uint64_t n = _reassembler.stream_out().bytes_written() + 1;
        n += static_cast<uint64_t>(_reassembler.stream_out().input_ended());
        return wrap(n, *_isn);
    }
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
