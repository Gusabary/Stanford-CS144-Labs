#include "stream_reassembler.hh"
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) 
    : _output(capacity), _capacity(capacity), _cur_num(0), _cur_idx(0), _eof_idx(0), _idx_to_substr() {}

static inline size_t str_end(const size_t index, const string &str) { return index + str.size(); }

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (str_end(index, data) <= _cur_idx) {
        // totally duplicated data
        if (eof) {
            _output.end_input();
        }
        return;
    }

    if (eof) {
        _eof_idx = str_end(index, data);
    }

    if (index > _cur_idx) {
        // data that cannot be reassembled immediately
        try_store_substr(index, data);
        return;
    }

    const size_t pos = _cur_idx - index;
    _output.write(data.substr(pos));
    if (eof) {
        _output.end_input();
    }

    const size_t size_reassembled = data.size() - pos;
    _cur_num += size_reassembled;
    _cur_idx += size_reassembled;

    try_load_substr();
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t ret = 0;
    for (auto it = _idx_to_substr.begin(); it != _idx_to_substr.end(); it++) {
        ret += it->second.size();
    }
    return ret;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }

void StreamReassembler::try_store_substr(const size_t index, const string &data) {
    const size_t data_end = str_end(index, data);
    size_t cur_index = index;
    
    while (cur_index < data_end) {
        size_t overlap_end = has_overlap_and_return_end(cur_index);
        if (overlap_end) {
            if (overlap_end >= data_end) {
                // totally duplicated
                return;
            }
            cur_index = overlap_end;
        }
        size_t cur_end = find_next_overlap_start(cur_index, data_end);

        if (_cur_num + (cur_end - cur_index) > _capacity) {
            cur_end = cur_index + (_capacity - _cur_num);
            _idx_to_substr.insert({cur_index, data.substr(cur_index - index, cur_end - cur_index)});
            _cur_num += (cur_end - cur_index);
            assert(_cur_num == _capacity);
            return;
        }

        _idx_to_substr.insert({cur_index, data.substr(cur_index - index, cur_end - cur_index)});
        _cur_num += (cur_end - cur_index);
        cur_index = cur_end;
    }
    assert(cur_index == data_end);
}

void StreamReassembler::try_load_substr() {
    auto it = _idx_to_substr.begin();
    for (; it != _idx_to_substr.end(); it++) {
        if (it->first <= _cur_idx) {
            if (str_end(it->first, it->second) <= _cur_idx) {
                continue;
            }
            size_t pos = _cur_idx - it->first;
            size_t size_to_write = it->second.size() - pos;
            string data_to_write = it->second.substr(pos, size_to_write);
            _output.write(data_to_write);
            _cur_idx += size_to_write;
            if (_cur_idx == _eof_idx) {
                _output.end_input();
            }
        }
        else {
            break;
        }
    }
    _idx_to_substr.erase(_idx_to_substr.begin(), it);
}

size_t StreamReassembler::has_overlap_and_return_end(size_t index) {
    auto upper = _idx_to_substr.upper_bound(index);
    if (upper == _idx_to_substr.begin() || 
        str_end(prev(upper)->first, prev(upper)->second) <= index) {
        return 0;
    }
    upper--;
    assert(upper->first <= index && str_end(upper->first, upper->second) > index);
    size_t cur_substr_end;
    while (true) {
        cur_substr_end = str_end(upper->first, upper->second);
        upper++;
        if (upper == _idx_to_substr.end()) {
            break;
        }
        size_t next_substr_start = upper->first;
        if (next_substr_start > cur_substr_end) {
            break;
        }
        assert(next_substr_start == cur_substr_end);
    }
    return cur_substr_end;
}

size_t StreamReassembler::find_next_overlap_start(size_t index, size_t end_pos) {
    auto upper = _idx_to_substr.upper_bound(index);
    if (upper == _idx_to_substr.end() || upper->first >= end_pos) {
        return end_pos;
    }
    return upper->first;
}

