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

#include <string>
#include <seastar/net/arp.hh>
#include <seastar/net/ip.hh>
#include <seastar/net/net.hh>
#include <seastar/core/reactor.hh>

// #include <seastar/net/virtio.hh>

using namespace seastar;
using namespace net;


void usage() {
    // std::cout<<"Usage: echotest [-virtio|-dpdk]"<<std::endl;
    // std::cout<<"   -virtio - use virtio backend (default)"<<std::endl;
    std::cout<<"   -dpdk   - use dpdk-pmd backend"<<std::endl;
}

int main(int ac, char** av) {
    std::unique_ptr<net::device> dnet;
    net::qp* vnet;

    boost::program_options::variables_map opts;
    opts.insert(std::make_pair("tap-device", boost::program_options::variable_value(std::string("tap0"), false)));

    if (ac > 2) {
        usage();
        return -1;
    }

    // if ((ac == 1) || !std::strcmp(av[1], "-virtio")) {
    //     dnet = create_virtio_net_device(opts);
    if (!std::strcmp(av[1], "-dpdk")) {
        dnet = create_dpdk_net_device();
    } else {
        usage();
        return -1;
    }

    auto qp = dnet->init_local_queue(opts, 0);
    vnet = qp.get();
    dnet->set_local_queue(std::move(qp));

    interface netif(std::move(vnet));
    ipv4 inet(&netif);
    inet.set_host_address(ipv4_address("192.168.122.2"));
    engine().run();
    return 0;
}



