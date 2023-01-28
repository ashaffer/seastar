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
/*-
 * Copyright (c) 2010 David Malone <dwmalone@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <vector>
#include <boost/crc.hpp>

namespace seastar {

using rss_key_type = uint8_t *;

typedef struct {
	uint8_t const *key;
	uint32_t keySize;
	uint32_t initial;
	bool full;
   bool sort;
} rss_config;

inline static void print_rss_conf (rss_config conf) {
   printf("rss conf: %u key size, 0x%x initial value, %u full, %u sort\n", conf.keySize, conf.initial, (uint)conf.full, (uint)conf.sort);
   printf("     key: ");

   for (uint i = 0; i < conf.keySize; i++) {
      printf(" %02.2x", (uint8_t)conf.key[i]);
   }

   printf("\n");
}

// Mellanox Linux's driver key
// static constexpr uint8_t default_rsskey_40bytes[] = {
//     0xd1, 0x81, 0xc6, 0x2c, 0xf7, 0xf4, 0xdb, 0x5b,
//     0x19, 0x83, 0xa2, 0xfc, 0x94, 0x3e, 0x1a, 0xdb,
//     0xd9, 0x38, 0x9e, 0x6b, 0xd1, 0x03, 0x9c, 0x2c,
//     0xa7, 0x44, 0x99, 0xad, 0x59, 0x3d, 0x56, 0xd9,
//     0xf3, 0x25, 0x3c, 0x06, 0x2a, 0xdc, 0x1f, 0xfc
// };

// Intel's i40e PMD default RSS key
static constexpr uint8_t default_rsskey_52bytes[] = {
    0x44, 0x39, 0x79, 0x6b, 0xb5, 0x4c, 0x50, 0x23,
    0xb6, 0x75, 0xea, 0x5b, 0x12, 0x4f, 0x9f, 0x30,
    0xb8, 0xa2, 0xc0, 0x3d, 0xdf, 0xdc, 0x4d, 0x02,
    0xa0, 0x8c, 0x9b, 0x33, 0x4a, 0xf6, 0x4a, 0x4c,
    0x05, 0xc6, 0xfa, 0x34, 0x39, 0x58, 0xd8, 0x55,
    0x7d, 0x99, 0x58, 0x3a, 0xe1, 0x38, 0xc9, 0x2e,
    0x81, 0x15, 0x03, 0x66
};

// AWS EC2 c5/z1d key
static constexpr uint8_t default_rsskey_40bytes[] = {
   0xbe, 0xac, 0x01, 0xfa, 0x6a, 0x42, 0xb7, 0x3b,
   0x80, 0x30, 0xf2, 0x0c, 0x77, 0xcb, 0x2d, 0xa3,
   0xae, 0x7b, 0x30, 0xb4, 0xd0, 0xca, 0x2b, 0xcb,
   0x43, 0xa3, 0x8f, 0xb0, 0x41, 0x67, 0x25, 0x3d,
   0x25, 0x5b, 0x0e, 0xc2, 0x6d, 0x5a, 0x56, 0xda
};

// AWS EC2 m5zn key
// static constexpr uint8_t default_rsskey_40bytes[] = {
//     0xed, 0x37, 0x50, 0xb2, 0xba, 0xa8, 0x64, 0xa4,
//     0x6f, 0x75, 0xce, 0x99, 0x07, 0xb5, 0x20, 0xaf,
//     0x36, 0xc3, 0x1e, 0x03, 0x46, 0x9a, 0x89, 0xa6,
//     0xe3, 0x86, 0x9c, 0x9e, 0x45, 0x5d, 0xac, 0xe6,
//     0xbe, 0x32, 0x80, 0x79, 0x8f, 0xf3, 0x4a, 0xd1
// };

// AWS EC2 c6i key
// static constexpr uint8_t default_rsskey_40bytes[] = {
//    0x4b, 0x8c, 0xeb, 0xcb, 0x2a, 0xcc, 0xd6, 0xd0,
//    0x8e, 0x12, 0xeb, 0x18, 0x46, 0xde, 0x8a, 0xea,
//    0x72, 0x8e, 0xb5, 0x75, 0xb3, 0x8b, 0x7e, 0x11,
//    0x4d, 0x21, 0x9e, 0xaf, 0x68, 0xaf, 0x84, 0x00,
//    0xc6, 0xdc, 0x0c, 0x13, 0x11, 0x4a, 0x06, 0xe4
// };


template<typename T>
static inline uint32_t
toeplitz_hash(const rss_config& config, const T& data)
{
	uint32_t hash = config.initial, v;
	auto key = config.key;
	u_int i, b;

	/* XXXRW: Perhaps an assertion about key length vs. data length? */

	v = (key[0]<<24) + (key[1]<<16) + (key[2] <<8) + key[3];
	for (i = 0; i < data.size(); i++) {
		for (b = 0; b < 8; b++) {
			if (data[i] & (1<<(7-b)))
				hash ^= v;
			v <<= 1;
			if ((i + 4) < config.keySize &&
			    (key[i+4] & (1<<(7-b))))
				v |= 1;
		}
	}

	return config.full
		? hash
		: (((hash) & 0xFFFF) << 16) | (hash & 0xFFFF);
}

}
