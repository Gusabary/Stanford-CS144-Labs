#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) 
    : _buffer(), _capacity(capacity), _in(0), _out(0), _ended(false) {}

size_t ByteStream::write(const string &data) {
    const size_t size_to_write = min(data.size(), remaining_capacity());
    _buffer.append(data, 0, size_to_write);
    _in += size_to_write;
    return size_to_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    return string{_buffer, _out, len};
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { _out += len; }

void ByteStream::end_input() { _ended = true; }

bool ByteStream::input_ended() const { return _ended; }

size_t ByteStream::buffer_size() const { return _in - _out; }

bool ByteStream::buffer_empty() const { return _in == _out; }

bool ByteStream::eof() const { return buffer_empty() && _ended; }

size_t ByteStream::bytes_written() const { return _in; }

size_t ByteStream::bytes_read() const { return _out; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
