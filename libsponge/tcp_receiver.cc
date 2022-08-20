#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader head = seg.header();

    if(!head.syn && !_syn_flag)
        return;
    
    string data = seg.payload().copy();
    bool eof = false;
    
    if(head.syn && !_syn_flag){
        _syn_flag = true;
        _isn = head.seqno;
        if(head.fin)
            _fin_flag = eof = true;
        _reassembler.push_substring(data,0,eof);
        return;
    }

    if(_syn_flag && head.fin)
        _fin_flag = eof = true;

    uint64_t checkpoint = _reassembler.ack_num();
    uint64_t abs_seqno = unwrap(head.seqno, _isn, checkpoint);
    uint64_t stream_seqno = abs_seqno - _syn_flag;

    _reassembler.push_substring(data, stream_seqno, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(!_syn_flag)
        return std::nullopt;
    return wrap(_reassembler.ack_num()+1+(_reassembler.empty()&&_fin_flag), _isn);
 }

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
