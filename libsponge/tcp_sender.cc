#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout{_initial_retransmission_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t ret = 0;
    for (auto it = _outstanding_segs.begin(); it != _outstanding_segs.end(); it++) {
        ret += it->length_in_sequence_space();
    }
    return ret;
}

void TCPSender::divide_and_send_segments(string payload, bool syn, bool fin) {
    while (!payload.empty() || syn || fin) {
        size_t max_payload_size = syn ? TCPConfig::MAX_PAYLOAD_SIZE - 1 : TCPConfig::MAX_PAYLOAD_SIZE;
        string payload_to_send = payload.substr(0, max_payload_size);
        size_t size_to_send = payload_to_send.size();

        bool has_remaining_space = false;
        if (size_to_send < max_payload_size) {
            has_remaining_space = true;
        }

        TCPSegment seg;
        seg.header().seqno = wrap(_next_seqno, _isn);
        seg.header().syn = syn;
        seg.header().fin = fin && has_remaining_space;
        seg.payload() = Buffer(move(payload_to_send));
        _segments_out.push(seg);
        _outstanding_segs.emplace_back(seg);
        _next_seqno += seg.length_in_sequence_space();

        payload = payload.substr(size_to_send);
        syn = false;
        if (has_remaining_space) {
            fin = false;
        }
    }

    if (!_timer.is_running()) {
        _timer.start(_retransmission_timeout);
    }
}

void TCPSender::fill_window() {
    if (_next_seqno == 0) {
        // send the first segment with SYN
        // assume that the first segment wouldn't contain any payload
        divide_and_send_segments("", true, false);
    } 
    else {
        string data_to_send = _stream.read(_window_right - _next_seqno);
        if ((data_to_send.size() < _window_right - _next_seqno) && _stream.input_ended() && !_fin_sent) {
            divide_and_send_segments(data_to_send, false, true);
            _fin_sent = true;
        }
        else if (!data_to_send.empty()) {
            divide_and_send_segments(data_to_send, false, false);
        }
    }
}

void TCPSender::try_to_ack_segments(const uint64_t ackno_absolute) {
    auto it = _outstanding_segs.begin();
    for (; it != _outstanding_segs.end(); it++) {
        uint64_t upper = unwrap(it->header().seqno, _isn, _next_seqno) + it->length_in_sequence_space();
        if (upper > ackno_absolute) {
            // the first segment that cannot be fully acked
            break;
        }
    }
    bool has_new_ack = (it != _outstanding_segs.begin());
    _outstanding_segs.erase(_outstanding_segs.begin(), it);

    if (_outstanding_segs.empty()) {
        // all outstanding data has been acked
        _timer.turn_off();
    }

    if (has_new_ack) {
        _retransmission_timeout = _initial_retransmission_timeout;
        if (!_outstanding_segs.empty()) {
            _timer.start(_retransmission_timeout);
        }
        _rx_attempts = 0;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_left = unwrap(ackno, _isn, _next_seqno);  // use _next_seqno as checkpoint
    _window_right = _window_left + window_size;
    try_to_ack_segments(_window_left);
    return (_window_left <= _next_seqno);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_timer.tick(ms_since_last_tick)) {
        TCPSegment seg_to_rx = _outstanding_segs.front();
        _segments_out.push(seg_to_rx);
        if (_window_right - _window_left > 0) {
            _rx_attempts++;
            _retransmission_timeout *= 2;
        }
        _timer.start(_retransmission_timeout);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _rx_attempts; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
