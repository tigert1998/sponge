#include "tcp_connection.hh"

#include <iostream>

void TCPConnection::send_segment(const TCPSegment &seg) {
    auto new_seg = seg;
    if (_receiver.ackno() != std::nullopt) {
        new_seg.header().ack = true;
        new_seg.header().ackno = *_receiver.ackno();
    }
    _segments_out.push(new_seg);
}

void TCPConnection::pop_and_send_all_segments() {
    while (!_sender.segments_out().empty()) {
        send_segment(_sender.segments_out().front());
        _sender.segments_out().pop();
    }
}

void TCPConnection::send_rst() {
    while (!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    _sender.send_empty_segment();
    auto segment = _sender.segments_out().front();
    _sender.segments_out().pop();

    segment.header().rst = true;

    send_segment(segment);
    _active = false;
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _ms - _segment_received_ms; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _segment_received_ms = _ms;
    if (seg.header().rst) {
        _active = false;
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
    } else {
        _receiver.segment_received(seg);
        if (seg.header().ack) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }

        if (_receiver.ackno() != std::nullopt) {
            _sender.fill_window();
            if (_sender.segments_out().empty() && seg.length_in_sequence_space() >= 1) {
                _sender.send_empty_segment();
            }
            pop_and_send_all_segments();
        }
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const std::string &data) {
    auto ret = _sender.stream_in().write(data);
    _sender.fill_window();
    pop_and_send_all_segments();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _ms += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() <= TCPConfig::MAX_RETX_ATTEMPTS) {
        pop_and_send_all_segments();
    } else {
        send_rst();
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    pop_and_send_all_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();
    pop_and_send_all_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            std::cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst();
        }
    } catch (const std::exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
