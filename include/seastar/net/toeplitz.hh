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

using rss_key_type = compat::basic_string_view<uint8_t>;

// Mellanox Linux's driver key
static constexpr uint8_t default_rsskey_40bytes[] = {
    0xd1, 0x81, 0xc6, 0x2c, 0xf7, 0xf4, 0xdb, 0x5b,
    0x19, 0x83, 0xa2, 0xfc, 0x94, 0x3e, 0x1a, 0xdb,
    0xd9, 0x38, 0x9e, 0x6b, 0xd1, 0x03, 0x9c, 0x2c,
    0xa7, 0x44, 0x99, 0xad, 0x59, 0x3d, 0x56, 0xd9,
    0xf3, 0x25, 0x3c, 0x06, 0x2a, 0xdc, 0x1f, 0xfc
};

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

// template<typename T>
// static inline uint32_t
// toeplitz_hash(rss_key_type key, const T& data)
// {
// 	uint32_t hash = 0, v;
// 	u_int i, b;

// 	/* XXXRW: Perhaps an assertion about key length vs. data length? */

// 	v = (key[0]<<24) + (key[1]<<16) + (key[2] <<8) + key[3];
// 	for (i = 0; i < data.size(); i++) {
// 		for (b = 0; b < 8; b++) {
// 			if (data[i] & (1<<(7-b)))
// 				hash ^= v;
// 			v <<= 1;
// 			if ((i + 4) < key.size() &&
// 			    (key[i+4] & (1<<(7-b))))
// 				v |= 1;
// 		}
// 	}
// 	return (hash);
// }

template<typename T>
static inline uint32_t
crc32_hash(const T& data)
{
	boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true, true> result;
	result.process_bytes(data.data, data.size());
	return result.checksum();	
}

template<typename T>
static inline uint32_t
crc32_hash1(const T& data)
{
	boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true, false> result;
	result.process_bytes(data.data, data.size());
	return result.checksum();	
}

template<typename T>
static inline uint32_t
crc32_hash2(const T& data)
{
	boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, false, true> result;
	result.process_bytes(data.data, data.size());
	return result.checksum();	
}

template<typename T>
static inline uint32_t
crc32_hash3(const T& data)
{
	boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, false, false> result;
	result.process_bytes(data.data, data.size());
	return result.checksum();	
}

template<typename T>
static inline uint32_t
crc32_hash4(const T& data)
{
	boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0x00000000, true, true> result;
	result.process_bytes(data.data, data.size());
	return result.checksum();	
}

template<typename T>
static inline uint32_t
crc32_hash5(const T& data)
{
	boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0x00000000, true, false> result;
	result.process_bytes(data.data, data.size());
	return result.checksum();	
}

template<typename T>
static inline uint32_t
crc32_hash6(const T& data)
{
	boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0x00000000, false, true> result;
	result.process_bytes(data.data, data.size());
	return result.checksum();	
}

template<typename T>
static inline uint32_t
crc32_hash7(const T& data)
{
	boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0x00000000, false, false> result;
	result.process_bytes(data.data, data.size());
	return result.checksum();	
}

#define POLY 0x8408
static inline unsigned short crc16(const char *data_p, unsigned short length, uint16_t poly=POLY)
{
      unsigned char i;
      unsigned int data;
      unsigned int crc = 0xffff;

      if (length == 0)
            return (~crc);

      do
      {
            for (i=0, data=(unsigned int)0xff & *data_p++;
                 i < 8; 
                 i++, data >>= 1)
            {
                  if ((crc & 0x0001) ^ (data & 0x0001))
                        crc = (crc >> 1) ^ poly;
                  else  crc >>= 1;
            }
      } while (--length);

      crc = ~crc;
      data = crc;
      crc = (crc << 8) | (data >> 8 & 0xff);

      return (crc);
}

static inline uint32_t
rc_crc32(uint32_t crc, const char *buf, size_t len, uint32_t poly=0xDEADBEEF)
{
	static uint32_t table[256];
	// static int have_table = 0;
	uint32_t rem;
	uint8_t octet;
	int i, j;
	const char *p, *q;
 
	/* This check is not thread safe; there is no mutex. */
	// if (have_table == 0) {
		/* Calculate CRC table. */
		for (i = 0; i < 256; i++) {
			rem = i;  /* remainder from polynomial division */
			for (j = 0; j < 8; j++) {
				if (rem & 1) {
					rem >>= 1;
					rem ^= poly;
				} else
					rem >>= 1;
			}
			table[i] = rem;
		}
	// 	have_table = 1;
	// }
 
	crc = ~crc;
	q = buf + len;
	for (p = buf; p < q; p++) {
		octet = *p;  /* Cast to unsigned octet. */
		crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
	}
	return ~crc;
}

static inline uint32_t
brute_crc32(uint32_t crc, const char *buf, size_t len, uint32_t target) {
	for (uint i = 0; i < 0xFFFFFFFF; i++) {
		uint32_t res = rc_crc32(crc, buf, len, i);
		if (res == target) {
			printf("Polynomial crc32 found: 0x%x\n", i);
			return res;
		}

		if (i % 1000000 == 0) {
			printf("%u crc32 polynomials checked\n", i);
		}
	}

	return -1;
}

static inline void
brute_crc16(const char *buf, size_t len, uint16_t target) {
	for (uint16_t i = 0; i < 0xFFFF; i++) {
		uint16_t res = crc16(buf, len, i);

		if (res == target) {
			printf("Polynomial crc16 found: 0x%x\n", i);
		}
	}
}
	
}
