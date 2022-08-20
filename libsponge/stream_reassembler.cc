#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _head_idx(0), _unassembled_bytes(0), _eof(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(index >= _head_idx+_capacity)
        return;
    if(eof)
        _eof = true;
    seg_node cur;
    if(index+data.size() <= _head_idx){
        if(_eof && empty())
            _output.end_input();
        return;
    }

    // new a segment node
    if(index < _head_idx){
        size_t offset = _head_idx - index;
        cur.data.assign(data.begin()+offset, data.end());
        cur.begin = _head_idx;
        cur.len = cur.data.size();
    }
    else{
        cur.data = data;
        cur.begin = index;
        cur.len = data.size();
    }
    _unassembled_bytes += cur.len;

    // merge segments
    long merged_bytes = 0;
    auto iter = _segs.lower_bound(cur);
    // merge pre
    while(iter != _segs.end() && (merged_bytes=merge_seg(cur,*iter)) >= 0){
        _segs.erase(iter);
        _unassembled_bytes -= merged_bytes;
        iter = _segs.lower_bound(cur);
    }
    //merge next
    while(iter != _segs.begin()){
        iter--;
        if((merged_bytes=merge_seg(cur,*iter)) < 0)
            break;
        _segs.erase(iter);
        _unassembled_bytes -= merged_bytes;
        iter = _segs.lower_bound(cur);
    }
    _segs.insert(cur);

    // write to Byte_stream
    if(!_segs.empty() && _segs.begin()->begin==_head_idx){
        size_t write_bytes = _output.write(_segs.begin()->data);
        _unassembled_bytes -= write_bytes;
        _head_idx += write_bytes;
        _segs.erase(_segs.begin());
    }

    // end input
    if(_eof && empty())
            _output.end_input();    
}

long StreamReassembler::merge_seg(seg_node &a, const seg_node &b){
    seg_node pre, next;
    if(a.begin < b.begin){
        pre = a;
        next = b;
    }
    else{
        pre = b;
        next = a;
    }
    if(next.begin > pre.begin+pre.len)
        return -1;
    if(pre.begin+pre.len >= next.begin+next.len){
        a = pre;
        return next.len;
    }
    a.begin = pre.begin;
    a.data = pre.data + next.data.substr(pre.begin+pre.len-next.begin);
    a.len = a.data.size();
    return pre.begin+pre.len-next.begin;
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }

size_t StreamReassembler::ack_num() const { return _head_idx; }