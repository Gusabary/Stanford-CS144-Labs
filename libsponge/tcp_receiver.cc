#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader header = seg.header();
    if (!_isn.has_value()) {
        if (header.syn) {
            _isn = header.seqno;
            _ackno = _isn.value();
        } else {
            return false;
        }
    }

    bool ret = true;
    const uint64_t unwrapped_seqno = unwrap(header.seqno, _isn.value(), _checkpoint);
    const uint64_t unwrapped_ackno = unwrap(_ackno, _isn.value(), _checkpoint);
    if (unwrapped_seqno + seg.length_in_sequence_space() <= unwrapped_ackno ||
        unwrapped_seqno >= unwrapped_ackno + window_size()) {
        // no overlap between segment and window
        ret = false;
    }

    const size_t size_before_push = stream_out().buffer_size();

    const string payload = seg.payload().copy();
    const size_t index = header.syn ? unwrapped_seqno : (unwrapped_seqno - 1);
    _checkpoint = index;
    const bool eof = header.fin;
    _reassembler.push_substring(payload, index, eof);

    const size_t size_after_push = stream_out().buffer_size();

    if (header.syn) {
        _ackno = _ackno + 1;
    }
    _ackno = _ackno + (size_after_push - size_before_push);
    if (eof) {
        _ackno = _ackno + 1;
    }

    return ret;
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _isn.has_value() ? optional<WrappingInt32>{_ackno} : nullopt; }

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
