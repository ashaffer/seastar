/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 */

#pragma once

#include <array>
#include <assert.h>
#include <algorithm>
#include <seastar/net/byteorder.hh>

namespace seastar {

namespace net {

struct ethernet_address {
    ethernet_address()
        : mac{} {}

    ethernet_address(const uint8_t *eaddr) {
        std::copy(eaddr, eaddr + 6, mac.begin());
    }

    ethernet_address(std::initializer_list<uint8_t> eaddr) {
        assert(eaddr.size() == mac.size());
        std::copy(eaddr.begin(), eaddr.end(), mac.begin());
    }

    std::array<uint8_t, 6> mac;

    template <typename Adjuster>
    void adjust_endianness(Adjuster a) {}

    static ethernet_address read(const char* p) {
        ethernet_address ea;
        std::copy_n(p, size(), reinterpret_cast<char*>(ea.mac.data()));\
        return ea;
    }
    static ethernet_address consume(const char*& p) {
        auto ea = read(p);
        p += size();
        return ea;
    }
    void write(char* p) const {
        std::copy_n(reinterpret_cast<const char*>(mac.data()), size(), p);
    }
    void produce(char*& p) const {
        write(p);
        p += size();
    }
    static constexpr size_t size() {
        return 6;
    }

    friend std::ostream& operator<<(std::ostream& os, const ethernet_address&& ea) {
        const auto& m = ea.mac;
        using u = const uint32_t;
        return os << std::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                u(m[0]), u(m[1]), u(m[2]), u(m[3]), u(m[4]), u(m[5]));
    }


    std::string to_string () {
        char s[32] = {0};

        sprintf(s, "%02x:%02x:%02x:%02x:%02x:%02x",
            (uint8_t)mac[0],
            (uint8_t)mac[1],
            (uint8_t)mac[2],
            (uint8_t)mac[3],
            (uint8_t)mac[4],
            (uint8_t)mac[5]
        );

        return s;
    }

} __attribute__((packed));

struct ethernet {
    using address = ethernet_address;
    static address broadcast_address() {
        return  {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    }
    static constexpr uint16_t arp_hardware_type() { return 1; }
};

struct eth_hdr {
    ethernet_address dst_mac;
    ethernet_address src_mac;
    ::seastar::net::packed<uint16_t> eth_proto;
    template <typename Adjuster>
    auto adjust_endianness(Adjuster a) {
        return a(eth_proto);
    }
} __attribute__((packed));

ethernet_address parse_ethernet_address(std::string addr);
};

};
