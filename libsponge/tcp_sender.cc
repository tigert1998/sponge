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

uint64_t TCPSender::bytes_in_flight() const { return {}; }

void TCPSender::fill_window() {}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    bool new_data = false;
    if (_last_ackno.has_value()) {
        uint64_t current_ackno = unwrap(ackno, _isn, *_last_ackno);
        if (current_ackno > _last_ackno) {
            _last_ackno = current_ackno;
            _last_window_size = window_size;
            new_data = true;
        }
    } else {
        _last_ackno = ackno.raw_value();
        _last_window_size = window_size;
        new_data = true;
    }
    if (!new_data) {
        return;
    }

    _rto = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;

    std::vector<TCPSegment> segments;
    for (auto kv : _outstanding_segments) {
        TCPSegment segment = kv.second;
        if (unwrap(segment.header().seqno, _isn, *_last_ackno) + segment.length_in_sequence_space() > _last_ackno) {
            segments.push_back(segment);
        }
    }
    _outstanding_segments.clear();
    for (const auto &segment : segments) {
        _outstanding_segments.insert({_ms, segment});
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _ms += ms_since_last_tick;
    while (1) {
        if (_outstanding_segments.empty())
            return;
        auto iter = _outstanding_segments.begin();
        if (_ms - iter->first >= _rto) {
            auto segment = iter->second;
            _outstanding_segments.erase(iter);
            _segments_out.push(segment);
            _outstanding_segments.insert({_ms, segment});
            _consecutive_retransmissions += 1;
            _rto *= 2;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {}
