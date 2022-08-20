#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    _time_since_last_segment_received = 0;
    // if RST
    if(seg.header().rst == true){
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
    }
    // Give the segment to the TCPReceiver
    _receiver.segment_received(seg);

    // inbound stream ends before the TCPConnection has ever sent a fin seg
    if(check_inbound() && !_sender.stream_in().eof())
        _linger_after_streams_finish = false;
    
    if(seg.header().ack){
        _sender.ack_received(seg.header().ackno,seg.header().win);
        real_send();
    }

    if(seg.length_in_sequence_space() > 0){
        _sender.fill_window();
        bool is_send = real_send();
        // send empty seg
        if(is_send == false){
            _sender.send_empty_segment();
            TCPSegment ss = _sender.segments_out().front();
            _sender.segments_out().pop();
            set_ack_winsize(ss);
            _segments_out.push(ss);
        }
    }
 }

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if(data.size() == 0) return 0;
    size_t written_bytes = _sender.stream_in().write(data);
    _sender.fill_window();
    real_send();
    return written_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    // if new retransmit seg is generated
    if(!_sender.segments_out().empty()){
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_winsize(seg);
        // abort the connection
        if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _active = false;
            seg.header().rst = true;
        }
        _segments_out.push(seg);
    }

    if(check_inbound() && check_outbound()){
        if(!_linger_after_streams_finish || _time_since_last_segment_received >=  10*_cfg.rt_timeout)
            _active = false;
    }
 }

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // send fin flag
    _sender.fill_window();
    real_send();
}

void TCPConnection::connect() {
    _sender.fill_window();
    real_send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            send_rst();
            _active = false;
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

bool TCPConnection::real_send(){
    bool sended = false;
    while(!_sender.segments_out().empty()){
        sended = true;
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_winsize(seg);
        _segments_out.push(seg);
    }
    return sended;
}

void TCPConnection::set_ack_winsize(TCPSegment &seg){
    optional<WrappingInt32> ackno = _receiver.ackno();
    if(ackno.has_value()){
        seg.header().ack = true;
        seg.header().ackno = ackno.value();
    }
    seg.header().win = static_cast<uint16_t>(_receiver.window_size());
}

bool TCPConnection::check_inbound(){
    return _receiver.unassembled_bytes()==0 && _receiver.stream_out().input_ended();
}

bool TCPConnection::check_outbound(){
    return _sender.stream_in().eof() && _sender.bytes_in_flight()==0 && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written()+2;
}

void TCPConnection::send_rst(){
    _sender.send_empty_segment();
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    set_ack_winsize(seg);
    seg.header().rst = true;
    _segments_out.push(seg);
}