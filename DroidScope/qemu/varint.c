/* Copyright (C) 2007-2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#include <inttypes.h>
#include "varint.h"

// Define some constants for powers of two.
static const int k2Exp6 = 64;
static const uint32_t k2Exp7 = 128;
static const int k2Exp13 = 8192;
static const uint32_t k2Exp14 = 16384;
static const int k2Exp20 = (1 * 1024 * 1024);
static const uint32_t k2Exp21 = (2 * 1024 * 1024);
static const int k2Exp27 = (128 * 1024 * 1024);
static const uint32_t k2Exp28 = (256 * 1024 * 1024);
static const uint64_t k2Exp35 = (32LL * 1024LL * 1024LL * 1024LL);
static const uint64_t k2Exp42 = (4LL * 1024LL * 1024LL * 1024LL * 1024LL);

// Encodes the 64-bit value "value" using the varint encoding.  The varint
// encoding uses a prefix followed by some data bits.  The valid prefixes
// and the number of data bits are given in the table below.
//
// Prefix     Bytes  Data bits
// 0          1      7
// 10         2      14
// 110        3      21
// 1110       4      28
// 11110      5      35
// 111110     6      42
// 11111100   9      64
// 11111101   reserved
// 11111110   reserved
// 11111111   reserved
char *varint_encode(uint64_t value, char *buf) {
  if (value < k2Exp7) {
    *buf++ = value;
  } else if (value < k2Exp14) {
    *buf++ = (2 << 6) | (value >> 8);
    *buf++ = value & 0xff;
  } else if (value < k2Exp21) {
    *buf++ = (6 << 5) | (value >> 16);
    *buf++ = (value >> 8) & 0xff;
    *buf++ = value & 0xff;
  } else if (value < k2Exp28) {
    *buf++ = (0xe << 4) | (value >> 24);
    *buf++ = (value >> 16) & 0xff;
    *buf++ = (value >> 8) & 0xff;
    *buf++ = value & 0xff;
  } else if (value < k2Exp35) {
    *buf++ = (0x1e << 3) | (value >> 32);
    *buf++ = (value >> 24) & 0xff;
    *buf++ = (value >> 16) & 0xff;
    *buf++ = (value >> 8) & 0xff;
    *buf++ = value & 0xff;
  } else if (value < k2Exp42) {
    *buf++ = (0x3e << 2) | (value >> 40);
    *buf++ = (value >> 32) & 0xff;
    *buf++ = (value >> 24) & 0xff;
    *buf++ = (value >> 16) & 0xff;
    *buf++ = (value >> 8) & 0xff;
    *buf++ = value & 0xff;
  } else {
    *buf++ = (0x7e << 1);
    *buf++ = (value >> 56) & 0xff;
    *buf++ = (value >> 48) & 0xff;
    *buf++ = (value >> 40) & 0xff;
    *buf++ = (value >> 32) & 0xff;
    *buf++ = (value >> 24) & 0xff;
    *buf++ = (value >> 16) & 0xff;
    *buf++ = (value >> 8) & 0xff;
    *buf++ = value & 0xff;
  }
  return buf;
}

// Encodes the 35-bit signed value "value" using the varint encoding.
// The varint encoding uses a prefix followed by some data bits.  The
// valid prefixes and the number of data bits is given in the table
// below.
//
// Prefix     Bytes  Data bits
// 0          1      7
// 10         2      14
// 110        3      21
// 1110       4      28
// 11110      5      35
char *varint_encode_signed(int64_t value, char *buf) {
  if (value < k2Exp6 && value >= -k2Exp6) {
    *buf++ = value & 0x7f;
  } else if (value < k2Exp13 && value >= -k2Exp13) {
    *buf++ = (2 << 6) | ((value >> 8) & 0x3f);
    *buf++ = value & 0xff;
  } else if (value < k2Exp20 && value >= -k2Exp20) {
    *buf++ = (6 << 5) | ((value >> 16) & 0x1f);
    *buf++ = (value >> 8) & 0xff;
    *buf++ = value & 0xff;
  } else if (value < k2Exp27 && value >= -k2Exp27) {
    *buf++ = (0xe << 4) | ((value >> 24) & 0xf);
    *buf++ = (value >> 16) & 0xff;
    *buf++ = (value >> 8) & 0xff;
    *buf++ = value & 0xff;
  } else {
    *buf++ = (0x1e << 3);
    *buf++ = (value >> 24) & 0xff;
    *buf++ = (value >> 16) & 0xff;
    *buf++ = (value >> 8) & 0xff;
    *buf++ = value & 0xff;
  }
  return buf;
}
