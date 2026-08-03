/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Standalone ASCII / text processing utilities.
 *
 * This module keeps all character classification, case conversion, hex,
 * base32/base64 and percent-encoding logic in one self-contained file so that
 * callers do not need to link against libc's locale dependent ctype layer.
 * Every lookup is driven by const tables declared below; no global mutable
 * state is introduced, so the helpers are safe to call from any context.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define TEXT_TABLE_SIZE   256
#define BASE32_PAD_BIT    0x1F
#define BASE64_PAD_CHAR  '='
#define CRC16_POLY        0x8005
#define CRC32_POLY        0xEDB88320U
#define URI_MAX_ESCAPE    3

typedef enum {
    ASCII_CLASS_CONTROL = 0x01,
    ASCII_CLASS_SPACE   = 0x02,
    ASCII_CLASS_DIGIT   = 0x04,
    ASCII_CLASS_HEX     = 0x08,
    ASCII_CLASS_UPPER   = 0x10,
    ASCII_CLASS_LOWER   = 0x20,
    ASCII_CLASS_PUNCT   = 0x40,
    ASCII_CLASS_PRINT   = 0x80
} AsciiClass;

typedef enum {
    HEX_FORMAT_LOWER = 0,
    HEX_FORMAT_UPPER = 1,
    HEX_FORMAT_MAX
} HexFormat;

typedef struct {
    const char *token;
    uint32_t value;
} TextToken;

typedef enum {
    ESCAPE_MODE_NONE = 0,
    ESCAPE_MODE_NUMERIC,
    ESCAPE_MODE_NAMED,
    ESCAPE_MODE_MAX
} EscapeMode;

static const uint8_t g_asciiClassTable[TEXT_TABLE_SIZE] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x82, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0x8C, 0x8C, 0x8C, 0x8C, 0x8C, 0x8C, 0x8C, 0x8C, 0x8C, 0x8C, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
    0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xC0, 0xC0, 0xC0, 0xC0, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* Maps '0'-'9', 'A'-'F' and 'a'-'f' to their nibble value, 0xFF otherwise. */
static const uint8_t g_hexValueTable[TEXT_TABLE_SIZE] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

/* Identity table except 'A'-'Z' are folded to 'a'-'z'. */
static const uint8_t g_upperToLowerTable[TEXT_TABLE_SIZE] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

/* Identity table except 'a'-'z' are raised to 'A'-'Z'. */
static const uint8_t g_lowerToUpperTable[TEXT_TABLE_SIZE] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

/* Marks characters that must be percent-encoded in a URI path component. */
static const uint8_t g_uriEncodeTable[TEXT_TABLE_SIZE] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
};

static const char g_base64EncodeTable[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
};

static const int8_t g_base64DecodeTable[TEXT_TABLE_SIZE] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static const char g_base32EncodeTable[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '2', '3', '4', '5', '6', '7',
};

static const char g_hexUpperDigits[] = "0123456789ABCDEF";
static const char g_hexLowerDigits[] = "0123456789abcdef";

static const TextToken g_commonTokenTable[] = {
    { "max",      0x01 },
    { "min",      0x02 },
    { "avg",      0x03 },
    { "sum",      0x04 },
    { "total",    0x05 },
    { "count",    0x06 },
    { "enabled",  0x10 },
    { "disabled", 0x11 },
    { "pending",  0x12 },
    { "active",   0x13 },
    { "stopped",  0x14 },
    { "restart",  0x15 },
    { "reboot",   0x16 },
    { "shutdown", 0x17 },
    { "upgrade",  0x18 },
    { "rollback", 0x19 },
    { "idle",     0x20 },
    { "busy",     0x21 },
    { "unknown",  0x22 },
    { "invalid",  0x23 },
    { "valid",    0x24 },
    { "default",  0x25 },
    { "system",   0x30 },
    { "vendor",   0x31 },
    { "chipset",  0x32 },
    { "sys_prod", 0x33 },
    { "chip_prod",0x34 },
    { "ramdisk",  0x35 },
    { "recovery", 0x36 },
    { "factory",  0x37 },
    { "early",    0x40 },
    { "late",     0x41 },
    { "pre_init", 0x42 },
    { "post_init",0x43 },
    { "pre_start",0x44 },
    { "post_stop",0x45 },
    { "normal",   0x50 },
    { "critical", 0x51 },
    { "important",0x52 },
    { "background",0x53 },
    { "foreground",0x54 },
    { "fifo",     0x60 },
    { "socket",   0x61 },
    { "console",  0x62 },
    { "async",    0x63 },
    { "sync",     0x64 },
    { "once",     0x65 },
    { "period",   0x66 },
    { "trigger",  0x67 },
    { "condition",0x68 },
    { "sandbox",  0x70 },
    { "seccomp",  0x71 },
    { "selinux",  0x72 },
    { "capability",0x73 },
    { "secon",    0x74 },
    { "access",   0x75 },
    { "cgroup",   0x76 },
    { "namespace",0x77 },
};

static const char g_sizeUnitTable[][4] = {
    "B", "KB", "MB", "GB", "TB", "PB", "EB",
};

static const char g_timeUnitTable[][4] = {
    "ns", "us", "ms", "s", "m", "h", "d",
};

static bool IsValidClassBit(unsigned int classBit)
{
    return ((classBit == ASCII_CLASS_CONTROL) || (classBit == ASCII_CLASS_SPACE) ||
        (classBit == ASCII_CLASS_DIGIT) || (classBit == ASCII_CLASS_HEX) ||
        (classBit == ASCII_CLASS_UPPER) || (classBit == ASCII_CLASS_LOWER) ||
        (classBit == ASCII_CLASS_PUNCT) || (classBit == ASCII_CLASS_PRINT));
}

static bool ClassHasBit(unsigned char ch, unsigned int classBit)
{
    if (!IsValidClassBit(classBit)) {
        return false;
    }
    return ((g_asciiClassTable[ch] & classBit) != 0);
}

bool AsciiIsControl(unsigned char ch)
{
    return ClassHasBit(ch, ASCII_CLASS_CONTROL);
}

bool AsciiIsSpace(unsigned char ch)
{
    return ClassHasBit(ch, ASCII_CLASS_SPACE);
}

bool AsciiIsDigit(unsigned char ch)
{
    return ClassHasBit(ch, ASCII_CLASS_DIGIT);
}

bool AsciiIsHexDigit(unsigned char ch)
{
    return ClassHasBit(ch, ASCII_CLASS_HEX);
}

bool AsciiIsUpper(unsigned char ch)
{
    return ClassHasBit(ch, ASCII_CLASS_UPPER);
}

bool AsciiIsLower(unsigned char ch)
{
    return ClassHasBit(ch, ASCII_CLASS_LOWER);
}

bool AsciiIsAlpha(unsigned char ch)
{
    return (AsciiIsUpper(ch) || AsciiIsLower(ch));
}

bool AsciiIsAlnum(unsigned char ch)
{
    return (AsciiIsAlpha(ch) || AsciiIsDigit(ch));
}

bool AsciiIsPunct(unsigned char ch)
{
    return ClassHasBit(ch, ASCII_CLASS_PUNCT);
}

bool AsciiIsGraph(unsigned char ch)
{
    return (AsciiIsAlnum(ch) || AsciiIsPunct(ch));
}

bool AsciiIsPrint(unsigned char ch)
{
    return ClassHasBit(ch, ASCII_CLASS_PRINT);
}

bool AsciiIsBlank(unsigned char ch)
{
    return ((ch == ' ') || (ch == '\t'));
}

unsigned char AsciiToLower(unsigned char ch)
{
    return (unsigned char)g_upperToLowerTable[ch];
}

unsigned char AsciiToUpper(unsigned char ch)
{
    return (unsigned char)g_lowerToUpperTable[ch];
}

int AsciiHexToNibble(unsigned char ch)
{
    return (int)g_hexValueTable[ch];
}

char AsciiNibbleToHex(unsigned int nibble, HexFormat format)
{
    if (nibble >= 16U) {
        return '?';
    }
    if (format == HEX_FORMAT_LOWER) {
        return g_hexLowerDigits[nibble];
    }
    return g_hexUpperDigits[nibble];
}

bool TextIsHexString(const char *str)
{
    if (str == NULL) {
        return false;
    }
    size_t len = strlen(str);
    if (len == 0) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!AsciiIsHexDigit((unsigned char)str[i])) {
            return false;
        }
    }
    return true;
}

bool TextIsDecString(const char *str)
{
    if (str == NULL) {
        return false;
    }
    size_t len = strlen(str);
    if (len == 0) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!AsciiIsDigit((unsigned char)str[i])) {
            return false;
        }
    }
    return true;
}

bool TextIsAlnumString(const char *str)
{
    if (str == NULL) {
        return false;
    }
    size_t len = strlen(str);
    if (len == 0) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!AsciiIsAlnum((unsigned char)str[i])) {
            return false;
        }
    }
    return true;
}

bool TextIsBlankString(const char *str)
{
    if (str == NULL) {
        return false;
    }
    size_t len = strlen(str);
    if (len == 0) {
        return true;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!AsciiIsBlank((unsigned char)str[i])) {
            return false;
        }
    }
    return true;
}

bool TextHasControlChar(const char *str)
{
    if (str == NULL) {
        return false;
    }
    size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (AsciiIsControl((unsigned char)str[i])) {
            return true;
        }
    }
    return false;
}

void TextToUpper(char *str)
{
    if (str == NULL) {
        return;
    }
    size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        str[i] = (char)AsciiToUpper((unsigned char)str[i]);
    }
}

void TextToLower(char *str)
{
    if (str == NULL) {
        return;
    }
    size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        str[i] = (char)AsciiToLower((unsigned char)str[i]);
    }
}

char *TextTrimLeading(char *str)
{
    if (str == NULL) {
        return NULL;
    }
    char *cursor = str;
    while ((*cursor != '\0') && (AsciiIsSpace((unsigned char)*cursor))) {
        ++cursor;
    }
    return cursor;
}

void TextTrimTrailing(char *str)
{
    if (str == NULL) {
        return;
    }
    size_t len = strlen(str);
    while (len > 0) {
        if (!AsciiIsSpace((unsigned char)str[len - 1])) {
            break;
        }
        str[len - 1] = '\0';
        --len;
    }
}

char *TextTrim(char *str)
{
    if (str == NULL) {
        return NULL;
    }
    TextTrimTrailing(str);
    return TextTrimLeading(str);
}

int TextReplaceChar(char *str, char from, char to)
{
    if (str == NULL) {
        return -1;
    }
    int replaced = 0;
    size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] == from) {
            str[i] = to;
            ++replaced;
        }
    }
    return replaced;
}

size_t TextRemoveChar(char *str, char removed)
{
    if (str == NULL) {
        return 0;
    }
    size_t read = 0;
    size_t write = 0;
    size_t len = strlen(str);
    while (read < len) {
        if (str[read] != removed) {
            str[write] = str[read];
            ++write;
        }
        ++read;
    }
    str[write] = '\0';
    return (read - write);
}

size_t TextCountChar(const char *str, char target)
{
    if (str == NULL) {
        return 0;
    }
    size_t count = 0;
    size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] == target) {
            ++count;
        }
    }
    return count;
}

ssize_t TextIndexOfChar(const char *str, char target)
{
    if (str == NULL) {
        return -1;
    }
    size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] == target) {
            return (ssize_t)i;
        }
    }
    return -1;
}

ssize_t TextLastIndexOfChar(const char *str, char target)
{
    if (str == NULL) {
        return -1;
    }
    size_t len = strlen(str);
    for (size_t i = len; i > 0; --i) {
        if (str[i - 1] == target) {
            return (ssize_t)(i - 1);
        }
    }
    return -1;
}

void TextReverse(char *str)
{
    if (str == NULL) {
        return;
    }
    size_t len = strlen(str);
    if (len < 2) {
        return;
    }
    size_t left = 0;
    size_t right = len - 1;
    while (left < right) {
        char tmp = str[left];
        str[left] = str[right];
        str[right] = tmp;
        ++left;
        --right;
    }
}

size_t TextRemoveDuplicateChars(char *str)
{
    if (str == NULL) {
        return 0;
    }
    size_t read = 0;
    size_t write = 0;
    size_t len = strlen(str);
    bool seen[TEXT_TABLE_SIZE] = { false };
    while (read < len) {
        unsigned char ch = (unsigned char)str[read];
        if (!seen[ch]) {
            seen[ch] = true;
            str[write] = (char)ch;
            ++write;
        }
        ++read;
    }
    str[write] = '\0';
    return write;
}

/* Requires str to point to a writable buffer with capacity of at least width + 1 bytes. */
size_t TextPadLeft(char *str, size_t width, char padChar)
{
    if (str == NULL) {
        return 0;
    }
    size_t len = strlen(str);
    if (len >= width) {
        return len;
    }
    size_t pad = width - len;
    memmove(str + pad, str, len + 1);
    for (size_t i = 0; i < pad; ++i) {
        str[i] = padChar;
    }
    return width;
}

/* Requires str to point to a writable buffer with capacity of at least width + 1 bytes. */
size_t TextPadRight(char *str, size_t width, char padChar)
{
    if (str == NULL) {
        return 0;
    }
    size_t len = strlen(str);
    if (len >= width) {
        return len;
    }
    for (size_t i = len; i < width; ++i) {
        str[i] = padChar;
    }
    str[width] = '\0';
    return width;
}

/* Requires str to point to a writable buffer with capacity of at least maxLen + 1 bytes. */
void TextTruncateEllipsis(char *str, size_t maxLen)
{
    if (str == NULL) {
        return;
    }
    size_t len = strlen(str);
    if (len <= maxLen) {
        return;
    }
    if (maxLen < 3) {
        str[0] = '\0';
        return;
    }
    memcpy(str + maxLen - 3, "...", 3);
    str[maxLen] = '\0';
}

bool TextIsQuoted(const char *str)
{
    if (str == NULL) {
        return false;
    }
    size_t len = strlen(str);
    if (len < 2) {
        return false;
    }
    char first = str[0];
    char last = str[len - 1];
    return ((first == '"' && last == '"') || (first == '\'' && last == '\''));
}

/* Strips surrounding quotes when present; output is truncated to at most outSize - 1 bytes. */
size_t TextUnquote(const char *str, char *out, size_t outSize)
{
    if (str == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    if (!TextIsQuoted(str)) {
        size_t len = strlen(str);
        if (len >= outSize) {
            len = outSize - 1;
        }
        memcpy(out, str, len);
        out[len] = '\0';
        return len;
    }
    size_t len = strlen(str) - 2;
    if (len >= outSize) {
        len = outSize - 1;
    }
    memcpy(out, str + 1, len);
    out[len] = '\0';
    return len;
}

size_t TextQuote(const char *str, char *out, size_t outSize)
{
    if (str == NULL || out == NULL || outSize < 3) {
        return 0;
    }
    size_t len = strlen(str);
    if ((len + 2) >= outSize) {
        len = outSize - 3;
    }
    out[0] = '"';
    memcpy(out + 1, str, len);
    out[len + 1] = '"';
    out[len + 2] = '\0';
    return (len + 2);
}

uint32_t TextFindToken(const char *str)
{
    if (str == NULL) {
        return 0;
    }
    size_t tableSize = sizeof(g_commonTokenTable) / sizeof(g_commonTokenTable[0]);
    for (size_t i = 0; i < tableSize; ++i) {
        if (strcmp(str, g_commonTokenTable[i].token) == 0) {
            return g_commonTokenTable[i].value;
        }
    }
    return 0;
}

const char *TextFindTokenName(uint32_t value)
{
    size_t tableSize = sizeof(g_commonTokenTable) / sizeof(g_commonTokenTable[0]);
    for (size_t i = 0; i < tableSize; ++i) {
        if (g_commonTokenTable[i].value == value) {
            return g_commonTokenTable[i].token;
        }
    }
    return NULL;
}

size_t TextFormatUint(uint64_t value, char *out, size_t outSize)
{
    if (out == NULL || outSize == 0) {
        return 0;
    }
    char reversed[32];
    size_t digits = 0;
    if (value == 0) {
        reversed[digits++] = '0';
    } else {
        while (value > 0) {
            reversed[digits++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    size_t totalDigits = digits;
    if (digits >= outSize) {
        digits = outSize - 1;
    }
    for (size_t i = 0; i < digits; ++i) {
        out[i] = reversed[totalDigits - 1 - i];
    }
    out[digits] = '\0';
    return digits;
}

size_t TextFormatHexBytes(const uint8_t *bytes, size_t len, HexFormat format,
    char *out, size_t outSize)
{
    if (bytes == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    if (len > (outSize - 1) / 2) {
        len = (outSize - 1) / 2;
    }
    size_t written = 0;
    for (size_t i = 0; i < len; ++i) {
        unsigned int high = (bytes[i] >> 4) & 0x0F;
        unsigned int low = bytes[i] & 0x0F;
        out[written++] = AsciiNibbleToHex(high, format);
        out[written++] = AsciiNibbleToHex(low, format);
    }
    out[written] = '\0';
    return written;
}

size_t TextFormatByteSize(uint64_t bytes, char *out, size_t outSize)
{
    if (out == NULL || outSize == 0) {
        return 0;
    }
    const double base = 1024.0;
    double value = (double)bytes;
    size_t unitIndex = 0;
    size_t unitCount = sizeof(g_sizeUnitTable) / sizeof(g_sizeUnitTable[0]);
    while ((value >= base) && ((unitIndex + 1) < unitCount)) {
        value /= base;
        ++unitIndex;
    }
    if (unitIndex == 0) {
        return TextFormatUint(bytes, out, outSize);
    }
    return (size_t)snprintf(out, outSize, "%.2f%s", value, g_sizeUnitTable[unitIndex]);
}

size_t TextFormatDuration(uint64_t nanos, char *out, size_t outSize)
{
    if (out == NULL || outSize == 0) {
        return 0;
    }
    const uint64_t units[] = { 1ULL, 1000ULL, 1000000ULL, 1000000000ULL, 60000000000ULL,
        3600000000000ULL, 86400000000000ULL };
    size_t unitCount = sizeof(units) / sizeof(units[0]);
    size_t unitIndex = 0;
    for (size_t i = 1; i < unitCount; ++i) {
        if (nanos >= units[i]) {
            unitIndex = i;
        }
    }
    uint64_t value = nanos / units[unitIndex];
    return (size_t)snprintf(out, outSize, "%llu%s", (unsigned long long)value,
        g_timeUnitTable[unitIndex]);
}

static size_t AppendByte(char *out, size_t outSize, size_t written, unsigned char ch)
{
    if (written >= (outSize - 1)) {
        return written;
    }
    out[written++] = (char)ch;
    return written;
}

static size_t AppendEscape(char *out, size_t outSize, size_t written, unsigned char ch,
    EscapeMode mode)
{
    if (mode == ESCAPE_MODE_NONE) {
        return AppendByte(out, outSize, written, ch);
    }
    if (mode == ESCAPE_MODE_NAMED) {
        const char *name = NULL;
        switch (ch) {
            case '\\':
                name = "\\\\";
                break;
            case '"':
                name = "\\\"";
                break;
            case '\'':
                name = "\\'";
                break;
            case '\n':
                name = "\\n";
                break;
            case '\r':
                name = "\\r";
                break;
            case '\t':
                name = "\\t";
                break;
            default:
                name = NULL;
                break;
        }
        if (name != NULL) {
            size_t nameLen = strlen(name);
            if ((written + nameLen) >= outSize) {
                return written;
            }
            memcpy(out + written, name, nameLen);
            return written + nameLen;
        }
        if (!AsciiIsPrint(ch)) {
            return AppendEscape(out, outSize, written, ch, ESCAPE_MODE_NUMERIC);
        }
        return AppendByte(out, outSize, written, ch);
    }
    if (AsciiIsPrint(ch)) {
        return AppendByte(out, outSize, written, ch);
    }
    if ((written + 4) >= outSize) {
        return written;
    }
    out[written++] = '\\';
    out[written++] = 'x';
    out[written++] = AsciiNibbleToHex((ch >> 4) & 0x0F, HEX_FORMAT_LOWER);
    out[written++] = AsciiNibbleToHex(ch & 0x0F, HEX_FORMAT_LOWER);
    return written;
}

size_t TextEscape(const char *str, char *out, size_t outSize, EscapeMode mode)
{
    if (str == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    if (mode < ESCAPE_MODE_NONE || mode >= ESCAPE_MODE_MAX) {
        mode = ESCAPE_MODE_NUMERIC;
    }
    size_t written = 0;
    size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        size_t before = written;
        written = AppendEscape(out, outSize, written, (unsigned char)str[i], mode);
        if (written == before) {
            break;
        }
        if (written >= (outSize - 1)) {
            break;
        }
    }
    out[written] = '\0';
    return written;
}

static int ConsumeHexEscape(const char *str, size_t len, size_t *index)
{
    if ((*index + 2) >= len) {
        return -1;
    }
    int high = AsciiHexToNibble((unsigned char)str[*index + 1]);
    int low = AsciiHexToNibble((unsigned char)str[*index + 2]);
    if (high < 0 || low < 0) {
        return -1;
    }
    *index += 3;
    return (high << 4) | low;
}

size_t TextUnescape(const char *str, char *out, size_t outSize)
{
    if (str == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    size_t read = 0;
    size_t written = 0;
    size_t len = strlen(str);
    while (read < len && written < (outSize - 1)) {
        char ch = str[read];
        if (ch != '\\') {
            out[written++] = ch;
            ++read;
            continue;
        }
        if ((read + 1) >= len) {
            out[written++] = '\\';
            ++read;
            continue;
        }
        char next = str[read + 1];
        int value = -1;
        switch (next) {
            case 'n':
                value = '\n';
                read += 2;
                break;
            case 'r':
                value = '\r';
                read += 2;
                break;
            case 't':
                value = '\t';
                read += 2;
                break;
            case '\\':
                value = '\\';
                read += 2;
                break;
            case '"':
                value = '"';
                read += 2;
                break;
            case '\'':
                value = '\'';
                read += 2;
                break;
            case 'x':
                value = ConsumeHexEscape(str, len, &read);
                break;
            default:
                out[written++] = '\\';
                ++read;
                continue;
        }
        if (value < 0) {
            out[written++] = '\\';
            ++read;
            continue;
        }
        if (value == 0) {
            return 0;
        }
        out[written++] = (char)value;
    }
    out[written] = '\0';
    return written;
}

size_t TextUriEncode(const char *str, char *out, size_t outSize)
{
    if (str == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    size_t read = 0;
    size_t written = 0;
    size_t len = strlen(str);
    while (read < len) {
        unsigned char ch = (unsigned char)str[read];
        if (g_uriEncodeTable[ch] == 0) {
            if (written >= (outSize - 1)) {
                break;
            }
            out[written++] = (char)ch;
            ++read;
            continue;
        }
        if ((written + URI_MAX_ESCAPE) >= outSize) {
            break;
        }
        out[written++] = '%';
        out[written++] = AsciiNibbleToHex((ch >> 4) & 0x0F, HEX_FORMAT_UPPER);
        out[written++] = AsciiNibbleToHex(ch & 0x0F, HEX_FORMAT_UPPER);
        ++read;
    }
    out[written] = '\0';
    return written;
}

size_t TextUriDecode(const char *str, char *out, size_t outSize)
{
    if (str == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    size_t read = 0;
    size_t written = 0;
    size_t len = strlen(str);
    while (read < len && written < (outSize - 1)) {
        if (str[read] == '%') {
            int value = ConsumeHexEscape(str, len, &read);
            if (value > 0) {
                out[written++] = (char)value;
                continue;
            }
            if (value == 0) {
                return 0;
            }
        }
        out[written++] = str[read];
        ++read;
    }
    out[written] = '\0';
    return written;
}

static size_t Base64EncodeBlock(const uint8_t *src, size_t srcLen, char *dst)
{
    static const size_t blockSize = 3;
    static const size_t outSize = 4;
    uint32_t buffer = 0;
    size_t used = 0;
    for (size_t i = 0; i < srcLen; ++i) {
        buffer = (buffer << 8) | src[i];
        ++used;
    }
    buffer <<= (blockSize - srcLen) * 8;
    size_t written = 0;
    for (size_t i = 0; i < outSize; ++i) {
        if (used == 0 || i < used + 1) {
            dst[written++] = g_base64EncodeTable[(buffer >> (18 - i * 6)) & 0x3F];
        } else {
            dst[written++] = BASE64_PAD_CHAR;
        }
    }
    return written;
}

size_t Base64Encode(const uint8_t *src, size_t srcLen, char *out, size_t outSize)
{
    if (src == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    if (srcLen == 0) {
        out[0] = '\0';
        return 0;
    }
    size_t groupCount = srcLen / 3;
    size_t remainder = srcLen % 3;
    size_t blocks = groupCount + (remainder ? 1 : 0);
    if (blocks > (outSize - 1) / 4) {
        return 0;
    }
    size_t total = blocks * 4;
    size_t written = 0;
    for (size_t i = 0; i < groupCount; ++i) {
        size_t n = Base64EncodeBlock(src + i * 3, 3, out + written);
        written += n;
    }
    if (remainder) {
        size_t n = Base64EncodeBlock(src + groupCount * 3, remainder, out + written);
        written += n;
    }
    out[written] = '\0';
    return written;
}

size_t Base64Decode(const char *src, uint8_t *out, size_t outSize)
{
    if (src == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    size_t len = strlen(src);
    if (len % 4 != 0) {
        return 0;
    }
    size_t written = 0;
    for (size_t i = 0; i < len; i += 4) {
        int c0 = g_base64DecodeTable[(unsigned char)src[i]];
        int c1 = g_base64DecodeTable[(unsigned char)src[i + 1]];
        int c2 = g_base64DecodeTable[(unsigned char)src[i + 2]];
        int c3 = g_base64DecodeTable[(unsigned char)src[i + 3]];
        if (c0 < 0 || c1 < 0) {
            return 0;
        }
        size_t produced = 3;
        if (src[i + 2] == BASE64_PAD_CHAR) {
            if ((i + 4) != len || src[i + 3] != BASE64_PAD_CHAR) {
                return 0;
            }
            produced = 1;
            c2 = 0;
            c3 = 0;
        } else if (c2 < 0) {
            return 0;
        } else if (src[i + 3] == BASE64_PAD_CHAR) {
            if ((i + 4) != len) {
                return 0;
            }
            produced = 2;
            c3 = 0;
        } else if (c3 < 0) {
            return 0;
        }
        uint32_t triple = ((uint32_t)c0 << 18) | ((uint32_t)c1 << 12) |
            ((uint32_t)c2 << 6) | (uint32_t)c3;
        if ((written + produced) > outSize) {
            return 0;
        }
        if (produced >= 1) {
            out[written++] = (uint8_t)((triple >> 16) & 0xFF);
        }
        if (produced >= 2) {
            out[written++] = (uint8_t)((triple >> 8) & 0xFF);
        }
        if (produced >= 3) {
            out[written++] = (uint8_t)(triple & 0xFF);
        }
    }
    return written;
}

size_t Base32Encode(const uint8_t *src, size_t srcLen, char *out, size_t outSize)
{
    if (src == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    if (srcLen == 0) {
        out[0] = '\0';
        return 0;
    }
    size_t groups = srcLen / 5 + (srcLen % 5 ? 1 : 0);
    if (groups > (outSize - 1) / 8) {
        return 0;
    }
    uint32_t buffer = 0;
    int bitsLeft = 0;
    size_t written = 0;
    for (size_t i = 0; i < srcLen; ++i) {
        buffer = (buffer << 8) | src[i];
        bitsLeft += 8;
        while (bitsLeft >= 5) {
            int index = (int)((buffer >> (bitsLeft - 5)) & BASE32_PAD_BIT);
            out[written++] = g_base32EncodeTable[index];
            bitsLeft -= 5;
        }
    }
    if (bitsLeft > 0) {
        int index = (int)((buffer << (5 - bitsLeft)) & BASE32_PAD_BIT);
        out[written++] = g_base32EncodeTable[index];
    }
    /* RFC 4648 base32 uses the same '=' pad character as base64. */
    while ((written & 7) != 0) {
        out[written++] = BASE64_PAD_CHAR;
    }
    out[written] = '\0';
    return written;
}

uint16_t Crc16(const uint8_t *data, size_t len, uint16_t seed)
{
    uint16_t crc = seed;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ CRC16_POLY);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

uint32_t Crc32(const uint8_t *data, size_t len, uint32_t seed)
{
    uint32_t crc = seed ^ 0xFFFFFFFFU;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ CRC32_POLY;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

size_t TextFormatUnitsTable(const uint64_t *values, size_t count, const char *separator,
    char *out, size_t outSize)
{
    if (values == NULL || separator == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    size_t written = 0;
    for (size_t i = 0; i < count; ++i) {
        char part[32];
        size_t partLen = TextFormatUint(values[i], part, sizeof(part));
        if ((written + partLen) >= outSize) {
            break;
        }
        memcpy(out + written, part, partLen);
        written += partLen;
        if ((i + 1) < count) {
            size_t sepLen = strlen(separator);
            if ((written + sepLen) >= outSize) {
                break;
            }
            memcpy(out + written, separator, sepLen);
            written += sepLen;
        }
    }
    out[written] = '\0';
    return written;
}

size_t TextBytesToHexString(const uint8_t *data, size_t len, bool upper, char *out,
    size_t outSize)
{
    HexFormat format = upper ? HEX_FORMAT_UPPER : HEX_FORMAT_LOWER;
    return TextFormatHexBytes(data, len, format, out, outSize);
}

size_t TextHexStringToBytes(const char *hex, uint8_t *out, size_t outSize)
{
    if (hex == NULL || out == NULL || outSize == 0) {
        return 0;
    }
    size_t len = strlen(hex);
    if ((len & 1) != 0) {
        return 0;
    }
    size_t byteCount = len / 2;
    if (byteCount > outSize) {
        return 0;
    }
    for (size_t i = 0; i < byteCount; ++i) {
        int high = AsciiHexToNibble((unsigned char)hex[i * 2]);
        int low = AsciiHexToNibble((unsigned char)hex[i * 2 + 1]);
        if (high < 0 || low < 0) {
            return 0;
        }
        out[i] = (uint8_t)((high << 4) | low);
    }
    return byteCount;
}

bool TextIsSafeName(const char *str)
{
    if (str == NULL || str[0] == '\0') {
        return false;
    }
    size_t len = strlen(str);
    if (len > 255) {
        return false;
    }
    if (!AsciiIsAlpha((unsigned char)str[0])) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)str[i];
        if (!AsciiIsAlnum(ch) && ch != '_' && ch != '-' && ch != '.') {
            return false;
        }
    }
    return true;
}

