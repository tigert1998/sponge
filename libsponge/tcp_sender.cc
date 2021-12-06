#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{std::random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {
    _rto = _initial_retransmission_timeout;
}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    uint16_t window_size;
    if (_last_window_size.has_value()) {
        window_size = std::max(static_cast<uint16_t>(1), *_last_window_size);
    } else {
        window_size = 1;
    }

    bool syn = _next_seqno == 0;
    bool fin = _stream.eof() && _stream.buffer_size() <= window_size - syn - 1;

    uint64_t str_size = std::min(TCPConfig::MAX_PAYLOAD_SIZE, _stream.buffer_size());
    str_size = std::min(str_size, static_cast<uint64_t>(window_size - syn - fin));

    TCPSegment segment;
    segment.header().seqno = wrap(_next_seqno, _isn);
    segment.header().syn = syn;
    segment.header().fin = fin;
    segment.payload() = Buffer(_stream.read(str_size));

    if (_fin_sent || str_size + syn + fin == 0) {
        return;
    }
    if (fin) {
        _fin_sent = true;
    }

    _segments_out.push(segment);
    _outstanding_segments.emplace(_next_seqno, segment);

    _bytes_in_flight += str_size + syn + fin;
    _next_seqno += str_size + syn + fin;

    if (!_timer_ms.has_value()) {
        _timer_ms = _ms;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    bool new_data = false;
    if (_last_ackno.has_value()) {
        uint64_t current_ackno = unwrap(ackno, _isn, *_last_ackno);
        if (current_ackno > _last_ackno && current_ackno <= _next_seqno) {
            _last_ackno = current_ackno;
            _last_window_size = window_size;
            new_data = true;
        }
    } else if (unwrap(ackno, _isn, 0) <= _next_seqno) {
        _last_ackno = unwrap(ackno, _isn, 0);
        _last_window_size = window_size;
        new_data = true;
    }
    if (!new_data) {
        return;
    }

    _rto = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;

    std::vector<std::pair<uint64_t, TCPSegment>> segments;
    for (auto kv : _outstanding_segments) {
        TCPSegment segment = kv.second;
        uint64_t seqno = kv.first;
        if (seqno + segment.length_in_sequence_space() > _last_ackno) {
            segments.emplace_back(seqno, segment);
        }
    }
    _outstanding_segments.clear();
    _bytes_in_flight = 0;
    for (const auto &kv : segments) {
        _outstanding_segments.insert(kv);
        _bytes_in_flight += kv.second.length_in_sequence_space();
    }

    if (_outstanding_segments.empty()) {
        _timer_ms = std::nullopt;
    } else {
        _timer_ms = _ms;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _ms += ms_since_last_tick;
    if (!_timer_ms.has_value())
        return;
    if (_ms - *_timer_ms >= _rto) {
        _timer_ms = _ms;
        auto iter = _outstanding_segments.begin();
        auto segment = iter->second;
        _segments_out.push(segment);
        _consecutive_retransmissions += 1;
        _rto *= 2;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = wrap(_next_seqno, _isn);
    segment.header().syn = false;
    segment.header().fin = false;
    segment.payload() = Buffer("");
    _segments_out.push(segment);
}
