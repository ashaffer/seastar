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

#include <seastar/net/native-stack.hh>
#include "net/native-stack-impl.hh"
#include <seastar/net/net.hh>
#include <seastar/net/ip.hh>
#include <seastar/net/tcp-stack.hh>
#include <seastar/net/tcp.hh>
#include <seastar/net/udp.hh>
#include <seastar/net/virtio.hh>
#include <seastar/net/dpdk.hh>
#include <seastar/net/proxy.hh>
#include <seastar/net/dhcp.hh>
#include <seastar/net/config.hh>
#include <memory>
#include <queue>
#include <fstream>
#ifdef HAVE_OSV
#include <osv/firmware.hh>
#include <gnu/libc-version.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace seastar {

namespace net {

using namespace seastar;

void create_native_net_device(boost::program_options::variables_map opts) {

    bool deprecated_config_used = true;

    std::stringstream net_config;

    if ( opts.count("net-config")) {
        deprecated_config_used = false;
        net_config << opts["net-config"].as<std::string>();             
    }
    if ( opts.count("net-config-file")) {
        deprecated_config_used = false;
        std::fstream fs(opts["net-config-file"].as<std::string>());
        net_config << fs.rdbuf();
    }

    std::vector<std::shared_ptr<device>> devices;
    device_configs dev_cfgs;

    if ( deprecated_config_used) {
#ifdef SEASTAR_HAVE_DPDK
        if ( opts.count("dpdk-pmd")) {
             devices.push_back(create_dpdk_net_device(opts["dpdk-port-index"].as<unsigned>(), smp::count,
                !(opts.count("lro") && opts["lro"].as<std::string>() == "off"),
                !(opts.count("hw-fc") && opts["hw-fc"].as<std::string>() == "off")));
       } else 
#endif  
        devices.push_back(create_virtio_net_device(opts));
    }
    else {
        dev_cfgs = parse_config(net_config);
        // if ( dev_cfgs.size() > 1) {
        //     std::runtime_error("only one network interface is supported");
        // }
        uint16_t num_queues = smp::count; // / dev_cfgs.size();

        for ( auto&& device_config : dev_cfgs) {
            auto& hw_config = device_config.second.hw_cfg;   
#ifdef SEASTAR_HAVE_DPDK
            if ( hw_config.port_index || !hw_config.pci_address.empty() ) {
                auto dev = create_dpdk_net_device(hw_config, num_queues);
                std::shared_ptr<device> sdev(dev.release());
	            devices.push_back(sdev);
	        } else 
#endif  
            {
                (void)hw_config;        
                std::runtime_error("only DPDK supports new configuration format"); 
            }
        }
    }

    if (devices.size() > smp::count) {
        printf("You can't have more NICs than CPUs at the moment\n");
        exit(-1);
    }

    auto sem = std::make_shared<semaphore>(0);
    uint jj = 0; 
    for (auto sdev : devices) {
        printf("sdev test: %u\n", sdev->port_idx());
        for (unsigned i = 0; i < smp::count; i++) {
            smp::submit_to(i, [i, jj, opts, sdev] {
                auto qid = engine().cpu_id();

                if (qid < sdev->hw_queues_count()) {
                    auto qp = sdev->init_local_queue(opts, qid);
                    std::map<unsigned, float> cpu_weights;
                    for (unsigned i = sdev->hw_queues_count() + qid % sdev->hw_queues_count(); i < smp::count; i+= sdev->hw_queues_count()) {
                        cpu_weights[i] = 1;
                    }
                    cpu_weights[qid] = opts["hw-queue-weight"].as<float>();
                    qp->configure_proxies(cpu_weights);
                    sdev->set_local_queue(std::move(qp), qid);
                } else {
                    auto master_qid = qid % sdev->hw_queues_count();
                    auto master_cpuid = sdev->qid2cpuid(master_qid);
                    sdev->set_local_queue(create_proxy_net_device(master_cpuid, sdev.get(), sdev->port_idx()), qid);
                }

            }).then([sem, i, jj] {
                sem->signal();
            });
        }
        jj++;
    }
    sem->wait(smp::count * devices.size()).then([opts, devices, dev_cfgs] {
        auto sem = std::make_shared<semaphore>(0);

        for (auto sdev : devices) {
            sdev->link_ready().then([sem] {
                sem->signal();
            });
        }

        sem->wait(devices.size()).then([opts, devices, dev_cfgs] {
            for (unsigned i = 0; i < smp::count; i++) {
                smp::submit_to(i, [opts, devices, dev_cfgs] {
                    create_native_stack(opts, devices, dev_cfgs);
                });
            }
        });
    });
}

// native_network_stack
class native_network_stack : public network_stack {
public:
    static thread_local promise<std::unique_ptr<network_stack>> ready_promise;
private:
    std::unordered_map<socket_address, ipv4*> _inet_map;
    bool _dhcp = false;
    promise<> _config;
    timer<> _timer;

    future<> run_dhcp(bool is_renew = false, const dhcp::lease & res = dhcp::lease());
    void on_dhcp(ipv4 *inet, bool, const dhcp::lease &, bool);
    void set_ipv4_packet_filter(ipv4 *inet, ip_packet_filter* filter) {
        inet->set_packet_filter(filter);
    }
    using tcp4 = tcp<ipv4_traits>;
public:
    explicit native_network_stack(boost::program_options::variables_map opts, std::vector<std::shared_ptr<device>> devices, device_configs dev_cfgs);
    virtual server_socket listen(socket_address sa, listen_options opt) override;
    virtual ::seastar::socket socket(socket_address local = {}) override;
    virtual udp_channel make_udp_channel(const socket_address& addr) override;
    virtual future<> initialize() override;
    static future<std::unique_ptr<network_stack>> create(boost::program_options::variables_map opts) {
        if (engine().cpu_id() == 0) {
            create_native_net_device(opts);
        }
        return ready_promise.get_future();
    }
    virtual bool has_per_core_namespace() override { return true; };
    void arp_learn(ethernet_address l2, ipv4_address l3) {
        for (auto ii : _inet_map) {
            ii.second->learn(l2, l3);
        }
    }
    friend class native_server_socket_impl<tcp4>;
};

thread_local promise<std::unique_ptr<network_stack>> native_network_stack::ready_promise;

udp_channel
native_network_stack::make_udp_channel(const socket_address& addr) {
    return _inet_map[addr]->get_udp().make_channel(addr);
}

void
add_native_net_options_description(boost::program_options::options_description &opts) {
    opts.add(get_virtio_net_options_description());
#ifdef SEASTAR_HAVE_DPDK
    opts.add(get_dpdk_net_options_description());
#endif
}

native_network_stack::native_network_stack(boost::program_options::variables_map opts, std::vector<std::shared_ptr<device>> devices, device_configs dev_cfgs) {
    uint i = 0; 

    for (auto&& device_config : dev_cfgs) {
        interface *iface = new interface{std::move(devices[i])};
        ipv4 *inet = new ipv4{iface};
        auto& ip_config = device_config.second.ip_cfg;

        _dhcp = ip_config.dhcp;
        inet->get_udp().set_queue_size(opts["udpv4-queue-size"].as<int>());

        if (!_dhcp) {
            for (auto ip : ip_config.ip) {
                auto sa = ipv4_address(ip);
                inet->set_host_address(sa);
                _inet_map[(socket_address)sa] = inet;
            }
            // _inet.set_host_address(ipv4_address(_dhcp ? 0 : opts["host-ipv4-addr"].as<std::string>()));
            inet->set_gw_address(ipv4_address(ip_config.gateway));
            inet->set_netmask_address(ipv4_address(ip_config.netmask));
        }

        if (i == 0) {
            socket_address sa = {};
            _inet_map[sa] = inet;
        }

        ++i;
    }
}

server_socket
native_network_stack::listen(socket_address sa, listen_options opts) {
    assert(sa.as_posix_sockaddr().sa_family == AF_INET);
    return tcpv4_listen(_inet_map[sa]->get_tcp(), ntohs(sa.as_posix_sockaddr_in().sin_port), opts);
}

seastar::socket native_network_stack::socket(socket_address sa) {
    printf("Socket called: %s\n", inet_ntoa(sa.u.in.sin_addr));
    return tcpv4_socket(_inet_map[sa]->get_tcp());
}

using namespace std::chrono_literals;

future<> native_network_stack::run_dhcp(bool is_renew, const dhcp::lease& res) {
    auto sem = std::make_shared<semaphore>(0);

    for (auto ii : _inet_map) {
        auto inet = ii.second;
        dhcp d(*inet);
        // Hijack the ip-stack.
        auto f = d.get_ipv4_filter();
        smp::invoke_on_all([f, inet] {
            auto & ns = static_cast<native_network_stack&>(engine().net());
            ns.set_ipv4_packet_filter(inet, f);
        }).then([this, inet, d = std::move(d), is_renew, res]() mutable {
            net::dhcp::result_type fut = is_renew ? d.renew(res) : d.discover();
            return fut.then([this, inet, is_renew](bool success, const dhcp::lease & res) {
                return smp::invoke_on_all([inet] {
                    auto & ns = static_cast<native_network_stack&>(engine().net());
                    ns.set_ipv4_packet_filter(inet, nullptr);
                }).then(std::bind(&net::native_network_stack::on_dhcp, this, inet, success, res, is_renew));
            }).finally([d = std::move(d)] {});
        }).then([sem] () {
            sem->signal();
        });
    }

    return sem->wait(_inet_map.size());
}

void native_network_stack::on_dhcp(ipv4 *inet, bool success, const dhcp::lease & res, bool is_renew) {
    if (success) {
        inet->set_host_address(res.ip);
        inet->set_gw_address(res.gateway);
        inet->set_netmask_address(res.netmask);
    }
    // Signal waiters.
    if (!is_renew) {
        _config.set_value();
    }

    if (engine().cpu_id() == 0) {
        // And the other cpus, which, in the case of initial discovery,
        // will be waiting for us.
        for (unsigned i = 1; i < smp::count; i++) {
            smp::submit_to(i, [inet, success, res, is_renew]() {
                auto & ns = static_cast<native_network_stack&>(engine().net());
                ns.on_dhcp(inet, success, res, is_renew);
            });
        }
        if (success) {
            // And set up to renew the lease later on.
            _timer.set_callback(
                    [this, res]() {
                        _config = promise<>();
                        run_dhcp(true, res);
                    });
            _timer.arm(
                    std::chrono::duration_cast<steady_clock_type::duration>(
                            res.lease_time));
        }
    }
}

future<> native_network_stack::initialize() {
    return network_stack::initialize().then([this]() {
        if (!_dhcp) {
            return make_ready_future();
        }

        // Only run actual discover on main cpu.
        // All other cpus must simply for main thread to complete and signal them.
        if (engine().cpu_id() == 0) {
            run_dhcp();
        }
        return _config.get_future();
    });
}

void arp_learn(ethernet_address l2, ipv4_address l3)
{
    for (unsigned i = 0; i < smp::count; i++) {
        smp::submit_to(i, [l2, l3] {
            auto & ns = static_cast<native_network_stack&>(engine().net());
            ns.arp_learn(l2, l3);
        });
    }
}

void create_native_stack(boost::program_options::variables_map opts, std::vector<std::shared_ptr<device>> devices, device_configs dev_cfgs) { 
   native_network_stack::ready_promise.set_value(std::unique_ptr<network_stack>(std::make_unique<native_network_stack>(opts, devices, dev_cfgs)));
}

boost::program_options::options_description nns_options() {
    boost::program_options::options_description opts(
            "Native networking stack options");
    opts.add_options()
        ("net-config-file",
                boost::program_options::value<std::string>()->default_value(""),
                "net config file describing the NIC devices to use")
        ("tap-device",
                boost::program_options::value<std::string>()->default_value("tap0"),
                "tap device to connect to")
        ("host-ipv4-addr",
                boost::program_options::value<std::vector<std::string>>()->default_value({"192.168.122.2"}),
                "static IPv4 address to use")
        ("gw-ipv4-addr",
                boost::program_options::value<std::string>()->default_value("192.168.122.1"),
                "static IPv4 gateway to use")
        ("netmask-ipv4-addr",
                boost::program_options::value<std::string>()->default_value("255.255.255.0"),
                "static IPv4 netmask to use")
        ("udpv4-queue-size",
                boost::program_options::value<int>()->default_value(ipv4_udp::default_queue_size),
                "Default size of the UDPv4 per-channel packet queue")
        ("dhcp",
                boost::program_options::value<bool>()->default_value(true),
                        "Use DHCP discovery")
        ("hw-queue-weight",
                boost::program_options::value<float>()->default_value(1.0f),
                "Weighing of a hardware network queue relative to a software queue (0=no work, 1=equal share)")
#ifdef SEASTAR_HAVE_DPDK
        ("dpdk-pmd", "Use DPDK PMD drivers")
#endif
        ("lro",
                boost::program_options::value<std::string>()->default_value("on"),
                "Enable LRO")
        ;

    add_native_net_options_description(opts);
    return opts;
}

void register_native_stack() {
    register_network_stack("native", nns_options(), native_network_stack::create);
}
}

}
