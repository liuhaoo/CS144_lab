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
    , _stream(capacity)
    , _retransmission_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // The first TCP connection request, and the syn will use a seqnum
    if(!_syn_flag){
        TCPSegment seg;
        seg.header().syn = true;
        _syn_flag = true;
        send_segment(seg);
        return;
    }

    if(!_segs_outstanding.empty() && _segs_outstanding.front().header().syn)
        return;
    if(!_stream.buffer_size() && !_stream.eof())
        return;
    if(_fin_flag)
        return;
    size_t win = _window_size==0?1:_window_size;
    size_t remain;
    while((remain = win-(_next_seqno - _recv_ackno)) && !_fin_flag){
        size_t size = min(remain, TCPConfig::MAX_PAYLOAD_SIZE);
        size = min(size,_stream.buffer_size());
        TCPSegment seg;
        seg.payload() = Buffer{_stream.read(size)};
        //if the segment reach eof, set the fin_flag
        if(seg.length_in_sequence_space()<win && _stream.eof()){
            _fin_flag = true;
            seg.header().fin = true;
        }

        if(seg.length_in_sequence_space() == 0)
            break;
        send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    size_t abs_ackno = unwrap(ackno,_isn,_next_seqno);
    if(abs_ackno > _next_seqno)
        return;
    _window_size = window_size;
    if(abs_ackno <= _recv_ackno)
        return;
    _recv_ackno = abs_ackno;

    while(!_segs_outstanding.empty()){
        TCPSegment seg = _segs_outstanding.front();
        if(unwrap(seg.header().seqno,_isn,_next_seqno)+seg.length_in_sequence_space() <= abs_ackno){
            _segs_outstanding.pop();
            _bytes_in_flight -= seg.length_in_sequence_space();
        }
        else break;
    }

    fill_window();

    _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmission = 0;
    if(!_segs_outstanding.empty()){
        _timer_running = true;
        _timer = 0;
    }
 }

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if(!_timer_running)
        return;
    _timer += ms_since_last_tick;
    if(_timer >= _retransmission_timeout && !_segs_outstanding.empty()){
        _segments_out.push(_segs_outstanding.front());
        if(_window_size || _segs_outstanding.front().header().syn)
        {
            _consecutive_retransmission++;
            _retransmission_timeout *= 2;
        }
        _timer_running = true;
        _timer = 0;
    }
    if(_segs_outstanding.empty()){
        _timer_running = false;
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno,_isn);
    _segments_out.push(seg);
}

void TCPSender::send_segment(TCPSegment &seg){
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_out.push(seg);
    _segs_outstanding.push(seg);
    if(!_timer_running){
        _timer_running = true;
        _timer = 0;
    }
}
