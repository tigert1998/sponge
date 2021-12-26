#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>
#include <string>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    std::cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
              << ip_address.ip() << "\n";
}

void NetworkInterface::send_ipv4_frame(const InternetDatagram &dgram, const EthernetAddress &dst) {
    EthernetFrame frame;
    frame.header().dst = dst;
    frame.header().src = _ethernet_address;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.payload() = dgram.serialize();
    _frames_out.emplace(frame);
}

void NetworkInterface::send_arp_frame(uint16_t opcode,
                                      const EthernetAddress &target_ethernet_address,
                                      uint32_t target_ip_address) {
    ARPMessage arp_msg;
    arp_msg.opcode = opcode;
    arp_msg.sender_ethernet_address = _ethernet_address;
    arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    arp_msg.target_ethernet_address = target_ethernet_address;
    arp_msg.target_ip_address = target_ip_address;
    EthernetFrame frame;
    frame.header().dst = target_ethernet_address;
    frame.header().src = _ethernet_address;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.payload() = arp_msg.serialize();
    _frames_out.emplace(frame);
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    if (_cache.count(next_hop_ip) >= 1) {
        auto ethernet_address = _cache[next_hop_ip].first;
        send_ipv4_frame(dgram, ethernet_address);
    } else {
        if (_wait_queues.count(next_hop_ip) == 0 || _ms - _wait_queues[next_hop_ip].second >= 5 * 1000) {
            send_arp_frame(ARPMessage::OPCODE_REQUEST, ETHERNET_BROADCAST, next_hop_ip);
        }

        if (_wait_queues.count(next_hop_ip) == 0) {
            _wait_queues[next_hop_ip] = {std::vector{dgram}, _ms};
        } else {
            _wait_queues[next_hop_ip].first.push_back(dgram);
            _wait_queues[next_hop_ip].second = _ms;
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
std::optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return std::nullopt;
    }

    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) == ParseResult::NoError) {
            return dgram;
        } else {
            return std::nullopt;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        if (arp_msg.parse(frame.payload()) == ParseResult::NoError) {
            _cache[arp_msg.sender_ip_address] = {arp_msg.sender_ethernet_address, _ms};
            if (_wait_queues.count(arp_msg.sender_ip_address) >= 1) {
                for (const auto &dgram : _wait_queues[arp_msg.sender_ip_address].first) {
                    send_ipv4_frame(dgram, arp_msg.sender_ethernet_address);
                }
                _wait_queues.erase(arp_msg.sender_ip_address);
            }

            if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST &&
                arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
                send_arp_frame(ARPMessage::OPCODE_REPLY, arp_msg.sender_ethernet_address, arp_msg.sender_ip_address);
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _ms += ms_since_last_tick;

    std::vector<uint32_t> to_remove;
    for (auto kv : _cache) {
        if (_ms - kv.second.second >= 30 * 1000) {
            to_remove.push_back(kv.first);
        }
    }

    for (const auto &address : to_remove) {
        _cache.erase(address);
    }
}
