#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    _table.push_back(RouterItem{route_prefix,prefix_length,next_hop,interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // Your code here.
    if(dgram.header().ttl <= 1)
        return;
    uint32_t dst_ip = dgram.header().dst;
    bool route_found = false;
    RouterItem ritem;
    for(size_t i = 0; i < _table.size(); i++){
        if(prefix_equal(_table[i].route_prefix,dst_ip,_table[i].prefix_length)){
            if(!route_found || ritem.prefix_length < _table[i].prefix_length){
                ritem = _table[i];
                route_found = true;
            }
        }
    }
    if(!route_found)
        return;
    dgram.header().ttl--;
    if(ritem.next_hop.has_value())
        _interfaces[ritem.interface_num].send_datagram(dgram,ritem.next_hop.value());
    else _interfaces[ritem.interface_num].send_datagram(dgram,Address::from_ipv4_numeric(dgram.header().dst));
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

bool Router::prefix_equal(uint32_t prefix, uint32_t ip_addr, uint8_t len){
    uint32_t mask = (len==0) ? 0 : 0xffffffff << (32-len);
    return (prefix & mask) == (ip_addr & mask);
}
