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

#ifdef SEASTAR_HAVE_DPDK

#include <memory>
#include <seastar/net/config.hh>
#include <seastar/net/net.hh>
#include <seastar/core/sstring.hh>

namespace seastar {

std::unique_ptr<net::device> create_dpdk_net_device(
                                    uint16_t port_idx = 0,
                                    uint16_t num_queues = 1,
                                    bool use_lro = true,
                                    bool enable_fc = true);

std::unique_ptr<net::device> create_dpdk_net_device(
                                    const net::hw_config& hw_cfg, uint16_t num_queues);


boost::program_options::options_description get_dpdk_net_options_description();

namespace dpdk {
/**
 * @return Number of bytes needed for mempool objects of each QP.
 */
uint32_t qp_mempool_obj_size(bool hugetlbfs_membackend);
}

}

#endif // SEASTAR_HAVE_DPDK
