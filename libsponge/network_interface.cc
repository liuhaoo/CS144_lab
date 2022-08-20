#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    if(get_EthernetAdress(next_hop_ip).has_value())
        send_helper(get_EthernetAdress(next_hop_ip).value(),dgram);
    else ack_wait(next_hop_ip,dgram);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST)
        return nullopt;
    
    if(frame.header().type == EthernetHeader::TYPE_IPv4){
        InternetDatagram dgram;
        if(dgram.parse(Buffer(frame.payload())) == ParseResult::NoError)
            return dgram;
        else return nullopt;
    }

    if(frame.header().type == EthernetHeader::TYPE_ARP){
        ARPMessage arp;
        if(arp.parse(Buffer(frame.payload())) == ParseResult::NoError){
            insert_mapping(arp.sender_ip_address, arp.sender_ethernet_address);
            if(_waitting_list.find(arp.sender_ip_address) != _waitting_list.end())
                send_waitting_list(arp.sender_ip_address, arp.sender_ethernet_address);
            if(arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric())
                send_ARP_reply(arp.sender_ip_address, arp.sender_ethernet_address);
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    timer += ms_since_last_tick;
    auto iter = _table.begin();
    while(!_table.empty() && timer-iter->second.time_of_create >= MAX_MAPPING_TIME){
        _table.erase(iter->first);
        iter = _table.begin();
    }
    if(_table.empty())
        return;
    auto pre = iter;
    for(iter = _table.begin(); iter != _table.end(); iter++){
        if(timer-iter->second.time_of_create >= MAX_MAPPING_TIME){
            _table.erase(iter->first);
            iter = pre;
        }
        pre = iter;
    }
 }

optional<EthernetAddress> NetworkInterface::get_EthernetAdress(const uint32_t ip_address){
    optional<EthernetAddress> ret = nullopt;
    if(_table.find(ip_address) != _table.end())
        ret = _table[ip_address].eth;
    return ret;
}

void NetworkInterface::send_helper(const EthernetAddress &MAC, const InternetDatagram &dgram){
    EthernetFrame efram;
    efram.header().type = EthernetHeader::TYPE_IPv4;
    efram.header().src = _ethernet_address;
    efram.header().dst = MAC;
    efram.payload() = dgram.serialize();
    _frames_out.push(efram);
}

optional<NetworkInterface::waitting_payload> NetworkInterface::get_WaittingList(const uint32_t &ip_address){
    optional<waitting_payload> ret = nullopt;
    if(_waitting_list.find(ip_address) != _waitting_list.end())
        ret = _waitting_list[ip_address];
    return ret;
}

// accept the InternetDatagram and ack for EthernetAddr
void NetworkInterface::ack_wait(const uint32_t &ip_address, const InternetDatagram &dgram){
    optional<waitting_payload> waitting_list = get_WaittingList(ip_address);
    bool send_ARP = false;
    if(waitting_list.has_value()){
        waitting_payload &wl = waitting_list.value();
        wl.waitting_datagrams.push(dgram);
        send_ARP = (timer-wl.time_of_last_arp) >= NetworkInterface::MAX_RETX_WAITING_TIME;
    }
    else{
        waitting_payload newlist;
        newlist.time_of_last_arp = timer;
        newlist.waitting_datagrams.push(dgram);
        _waitting_list[ip_address] = newlist;
        send_ARP = true;
    }
    if(send_ARP) send_ARP_request(ip_address);
}

void NetworkInterface::send_ARP_request(const uint32_t &ip_address){
    EthernetFrame efram;
    efram.header().type = EthernetHeader::TYPE_ARP;
    efram.header().src = _ethernet_address;
    efram.header().dst = ETHERNET_BROADCAST;
    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ethernet_address = _ethernet_address;
    arp.sender_ip_address = _ip_address.ipv4_numeric();
    arp.target_ip_address = ip_address;
    efram.payload() = BufferList(arp.serialize());
    _frames_out.push(efram);
}

void NetworkInterface::insert_mapping(const uint32_t &ip_address, const EthernetAddress &eth){
    if(_table.find(ip_address) != _table.end()){
        arp_item &it = _table[ip_address];
        it.time_of_create = timer;
        it.eth = eth;
    }
    else{
        arp_item arp;
        arp.time_of_create = timer;
        arp.eth = eth;
        _table[ip_address] = arp;
    }
}

void NetworkInterface::send_waitting_list(const uint32_t &ip_address, const EthernetAddress &eth){
    std::queue<InternetDatagram> &que = _waitting_list[ip_address].waitting_datagrams;
    while(!que.empty()){
        InternetDatagram dgram = que.front();
        que.pop();
        send_helper(eth,dgram);
    }
    _waitting_list.erase(ip_address);
}

void NetworkInterface::send_ARP_reply(const uint32_t &ip_address, const EthernetAddress &eth){
    EthernetFrame efram;
    efram.header().type = EthernetHeader::TYPE_ARP;
    efram.header().src = _ethernet_address;
    efram.header().dst = eth;
    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REPLY;
    arp.sender_ethernet_address = _ethernet_address;
    arp.sender_ip_address = _ip_address.ipv4_numeric();
    arp.target_ip_address = ip_address;
    arp.target_ethernet_address = eth;
    efram.payload() = BufferList(arp.serialize());
    _frames_out.push(efram);
}