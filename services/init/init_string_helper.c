/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "init_string_helper.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "securec.h"

/* -------------------------------------------------------------------------- */
/* Internal helper macros and functions                                        */
/* -------------------------------------------------------------------------- */

#ifndef INIT_STATIC
#define INIT_STATIC static
#endif

/* 最小可反转字符串长度：长度为 0 或 1 的字符串无需反转 */
#define STRING_HELPER_MIN_REVERSE_LEN 2

/* 单次动态内存申请的大小上限，防止异常或溢出时申请过大内存 */
#define STRING_HELPER_MAX_ALLOC_SIZE (16 * 1024 * 1024)

INIT_STATIC bool CheckSizeOverflow(size_t base, size_t add)
{
    return (add > SIZE_MAX - base);
}

INIT_STATIC size_t StringHelperMin(size_t a, size_t b)
{
    return (a < b) ? a : b;
}

INIT_STATIC size_t StringHelperMax(size_t a, size_t b)
{
    return (a > b) ? a : b;
}

INIT_STATIC char *SafeRealloc(char *ptr, size_t oldSize, size_t newSize)
{
    /* 不使用 libc 的 realloc，改用 malloc + 安全拷贝 + free 保留旧数据 */
    if (newSize == 0 || newSize > STRING_HELPER_MAX_ALLOC_SIZE) {
        return NULL;
    }

    char *newPtr = (char *)malloc(newSize);
    if (newPtr == NULL) {
        return NULL;
    }

    if (ptr != NULL && oldSize > 0) {
        size_t copySize = StringHelperMin(oldSize, newSize);
        if (memcpy_s(newPtr, newSize, ptr, copySize) != 0) {
            free(newPtr);
            return NULL;
        }
    }

    free(ptr);
    return newPtr;
}

INIT_STATIC bool IsWhitespaceChar(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f');
}

INIT_STATIC bool IsAlphaNumericChar(char ch)
{
    return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'));
}

INIT_STATIC char ToLowerChar(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch + ('a' - 'A'));
    }
    return ch;
}

INIT_STATIC char ToUpperChar(char ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

INIT_STATIC int SafeStrLen(const char *str, size_t maxLen)
{
    if (str == NULL) {
        return -1;
    }
    size_t len = 0;
    while (len < maxLen && str[len] != '\0') {
        len++;
    }
    if (len >= maxLen) {
        return -1;
    }
    return (int)len;
}

INIT_STATIC bool IsStringPrintable(const char *str)
{
    if (str == NULL) {
        return false;
    }
    while (*str != '\0') {
        if (!isprint((unsigned char)*str) && !isspace((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}

INIT_STATIC size_t CountLeadingSpaces(const char *str)
{
    size_t count = 0;
    if (str == NULL) {
        return 0;
    }
    while (*str != '\0' && IsWhitespaceChar(*str)) {
        count++;
        str++;
    }
    return count;
}

INIT_STATIC size_t CountTrailingSpaces(const char *str)
{
    if (str == NULL) {
        return 0;
    }
    size_t len = strlen(str);
    size_t count = 0;
    while (len > 0 && IsWhitespaceChar(str[len - 1])) {
        count++;
        len--;
    }
    return count;
}

INIT_STATIC char *SkipWhitespaceForward(char *str)
{
    if (str == NULL) {
        return NULL;
    }
    while (*str != '\0' && IsWhitespaceChar(*str)) {
        str++;
    }
    return str;
}

INIT_STATIC char *SkipNonWhitespaceForward(char *str)
{
    if (str == NULL) {
        return NULL;
    }
    while (*str != '\0' && !IsWhitespaceChar(*str)) {
        str++;
    }
    return str;
}

/* -------------------------------------------------------------------------- */
/* Dynamic string buffer implementation                                        */
/* -------------------------------------------------------------------------- */

int InitDynamicStringBuffer(DynamicStringBuffer *buf, size_t initialCapacity, size_t growSizeIncrement)
{
    if (buf == NULL || initialCapacity == 0) {
        return -1;
    }

    buf->data = (char *)calloc(1, initialCapacity);
    if (buf->data == NULL) {
        return -1;
    }

    buf->capacity = initialCapacity;
    buf->length = 0;
    buf->growSize = (growSizeIncrement > 0) ? growSizeIncrement : STRING_HELPER_MEDIUM_BUF;
    buf->isOwned = true;
    buf->data[0] = '\0';
    return 0;
}

void ReleaseDynamicStringBuffer(DynamicStringBuffer *buf)
{
    if (buf == NULL) {
        return;
    }

    if (buf->isOwned && buf->data != NULL) {
        free(buf->data);
    }

    buf->data = NULL;
    buf->capacity = 0;
    buf->length = 0;
    buf->growSize = 0;
    buf->isOwned = false;
}

int DynamicStringBufferAppend(DynamicStringBuffer *buf, const char *src)
{
    if (buf == NULL || src == NULL || buf->data == NULL) {
        return -1;
    }

    size_t srcLen = strlen(src);
    if (CheckSizeOverflow(buf->length, srcLen)) {
        return -1;
    }
    size_t newLen = buf->length + srcLen;

    if (newLen + 1 > buf->capacity) {
        size_t needSize = StringHelperMax(buf->growSize, srcLen + 1);
        if (CheckSizeOverflow(buf->capacity, needSize)) {
            return -1;
        }
        size_t newCapacity = buf->capacity + needSize;
        char *newData = SafeRealloc(buf->data, buf->capacity, newCapacity);
        if (newData == NULL) {
            return -1;
        }
        buf->data = newData;
        buf->capacity = newCapacity;
    }

    if (memcpy_s(buf->data + buf->length, buf->capacity - buf->length, src, srcLen) != 0) {
        return -1;
    }
    buf->length = newLen;
    buf->data[buf->length] = '\0';
    return 0;
}

int DynamicStringBufferAppendFormat(DynamicStringBuffer *buf, const char *format, ...)
{
    if (buf == NULL || format == NULL || buf->data == NULL) {
        return -1;
    }

    va_list args;
    va_list argsCopy;

    va_start(args, format);
    va_copy(argsCopy, args);

    size_t remaining = buf->capacity - buf->length;
    int needed = vsnprintf_s(buf->data + buf->length, remaining, remaining, format, args);
    va_end(args);

    if (needed < 0) {
        va_end(argsCopy);
        return -1;
    }

    if ((size_t)needed >= remaining) {
        size_t needSize = StringHelperMax(buf->growSize, (size_t)needed + 1);
        if (CheckSizeOverflow(buf->capacity, needSize)) {
            va_end(argsCopy);
            return -1;
        }
        size_t newCapacity = buf->capacity + needSize;
        char *newData = SafeRealloc(buf->data, buf->capacity, newCapacity);
        if (newData == NULL) {
            va_end(argsCopy);
            return -1;
        }
        buf->data = newData;
        buf->capacity = newCapacity;

        remaining = buf->capacity - buf->length;
        vsnprintf_s(buf->data + buf->length, remaining, remaining, format, argsCopy);
    }
    va_end(argsCopy);

    buf->length += (size_t)needed;
    return 0;
}

void DynamicStringBufferClear(DynamicStringBuffer *buf)
{
    if (buf == NULL || buf->data == NULL) {
        return;
    }
    buf->length = 0;
    buf->data[0] = '\0';
}

int DynamicStringBufferReserve(DynamicStringBuffer *buf, size_t additionalSize)
{
    if (buf == NULL || buf->data == NULL) {
        return -1;
    }

    if (CheckSizeOverflow(buf->capacity, additionalSize)) {
        return -1;
    }
    size_t newCapacity = buf->capacity + additionalSize;
    char *newData = SafeRealloc(buf->data, buf->capacity, newCapacity);
    if (newData == NULL) {
        return -1;
    }

    buf->data = newData;
    buf->capacity = newCapacity;
    return 0;
}

int DynamicStringBufferShrinkToFit(DynamicStringBuffer *buf)
{
    if (buf == NULL || buf->data == NULL) {
        return -1;
    }

    size_t neededSize = buf->length + 1;
    if (neededSize >= buf->capacity) {
        return 0;
    }

    char *newData = SafeRealloc(buf->data, buf->capacity, neededSize);
    if (newData == NULL) {
        return 0; /* Shrink is optional, don't fail if realloc fails */
    }

    buf->data = newData;
    buf->capacity = neededSize;
    return 0;
}

/* -------------------------------------------------------------------------- */
/* String trimming and whitespace implementation                               */
/* -------------------------------------------------------------------------- */

INIT_STATIC void CollapseWhitespaceInPlace(char *str)
{
    char *readPtr = str;
    char *writePtr = str;
    bool prevSpace = false;

    while (*readPtr != '\0') {
        if (!IsWhitespaceChar(*readPtr)) {
            *writePtr++ = *readPtr;
            prevSpace = false;
            readPtr++;
            continue;
        }
        if (!prevSpace) {
            *writePtr++ = ' ';
            prevSpace = true;
        }
        readPtr++;
    }
    *writePtr = '\0';
}

INIT_STATIC bool TrimLeftInPlace(char *str, size_t *len)
{
    char *src = str;
    while (IsWhitespaceChar(*src)) {
        src++;
        (*len)--;
    }
    if (src == str) {
        return true;
    }
    return (memmove_s(str, (*len) + 1, src, (*len) + 1) == 0);
}

char *TrimString(char *str, TrimMode mode)
{
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str);

    if (mode & TRIM_MODE_RIGHT) {
        while (len > 0 && IsWhitespaceChar(str[len - 1])) {
            str[--len] = '\0';
        }
    }

    if (mode & TRIM_MODE_LEFT) {
        if (!TrimLeftInPlace(str, &len)) {
            return str;
        }
    }

    if (mode & TRIM_MODE_COLLAPSE) {
        CollapseWhitespaceInPlace(str);
    }

    return str;
}

char *TrimStringCopy(const char *str, TrimMode mode)
{
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str);
    size_t allocSize = len + 1;
    if (allocSize < len) {
        return NULL; /* 申请长度溢出校验 */
    }
    char *result = (char *)malloc(allocSize);
    if (result == NULL) {
        return NULL;
    }

    if (memcpy_s(result, allocSize, str, allocSize) != 0) {
        free(result);
        return NULL;
    }
    return TrimString(result, mode);
}

bool IsStringBlank(const char *str)
{
    if (str == NULL) {
        return true;
    }

    while (*str != '\0') {
        if (!IsWhitespaceChar(*str)) {
            return false;
        }
        str++;
    }
    return true;
}

size_t CountNonWhitespace(const char *str)
{
    if (str == NULL) {
        return 0;
    }

    size_t count = 0;
    while (*str != '\0') {
        if (!IsWhitespaceChar(*str)) {
            count++;
        }
        str++;
    }
    return count;
}

char *RemoveAllWhitespace(char *str)
{
    if (str == NULL) {
        return NULL;
    }

    char *readPtr = str;
    char *writePtr = str;

    while (*readPtr != '\0') {
        if (!IsWhitespaceChar(*readPtr)) {
            *writePtr++ = *readPtr;
        }
        readPtr++;
    }
    *writePtr = '\0';
    return str;
}

char *CollapseWhitespace(char *str)
{
    if (str == NULL) {
        return NULL;
    }

    char *readPtr = str;
    char *writePtr = str;
    bool prevSpace = false;

    /* Skip leading whitespace */
    while (IsWhitespaceChar(*readPtr)) {
        readPtr++;
    }

    while (*readPtr != '\0') {
        bool isSpace = IsWhitespaceChar(*readPtr);
        if (isSpace && !prevSpace) {
            *writePtr++ = ' ';
            prevSpace = true;
        } else if (!isSpace) {
            *writePtr++ = *readPtr;
            prevSpace = false;
        }
        readPtr++;
    }

    /* Remove trailing space */
    if (prevSpace && writePtr > str) {
        writePtr--;
    }

    *writePtr = '\0';
    return str;
}

/* -------------------------------------------------------------------------- */
/* String comparison and searching implementation                              */
/* -------------------------------------------------------------------------- */

INIT_STATIC int CompareFromStart(const char *str, const char *prefix, uint32_t flags)
{
    size_t len = strlen(prefix);
    if (flags & STR_MATCH_CASE_SENSITIVE) {
        return strncmp(str, prefix, len);
    }
    return strncasecmp(str, prefix, len);
}

INIT_STATIC int CompareFromEnd(const char *str, const char *suffix, uint32_t flags)
{
    size_t strLen = strlen(str);
    size_t suffixLen = strlen(suffix);
    if (suffixLen > strLen) {
        return 1;
    }
    const char *end = str + strLen - suffixLen;
    if (flags & STR_MATCH_CASE_SENSITIVE) {
        return strcmp(end, suffix);
    }
    return strcasecmp(end, suffix);
}

INIT_STATIC int CompareWholeWord(const char *str1, const char *str2, uint32_t flags)
{
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    if (len1 != len2) {
        return (len1 < len2) ? -1 : 1;
    }
    if (flags & STR_MATCH_CASE_SENSITIVE) {
        return strcmp(str1, str2);
    }
    return strcasecmp(str1, str2);
}

int CompareStringsWithFlags(const char *str1, const char *str2, uint32_t flags)
{
    if (str1 == NULL && str2 == NULL) {
        return 0;
    }
    if (str1 == NULL) {
        return -1;
    }
    if (str2 == NULL) {
        return 1;
    }

    const char *p1 = str1;
    const char *p2 = str2;

    /* Skip leading whitespace if flag is set */
    if (flags & STR_MATCH_IGNORE_WHITESPACE) {
        while (IsWhitespaceChar(*p1)) {
            p1++;
        }
        while (IsWhitespaceChar(*p2)) {
            p2++;
        }
    }

    if (flags & STR_MATCH_FROM_START) {
        return CompareFromStart(p1, p2, flags);
    }
    if (flags & STR_MATCH_FROM_END) {
        return CompareFromEnd(p1, p2, flags);
    }
    if (flags & STR_MATCH_WHOLE_WORD) {
        return CompareWholeWord(p1, p2, flags);
    }
    if (flags & STR_MATCH_CASE_SENSITIVE) {
        return strcmp(p1, p2);
    }
    return strcasecmp(p1, p2);
}

INIT_STATIC bool IsCharInSet(char ch, const char *charSet)
{
    while (*charSet != '\0') {
        if (ch == *charSet) {
            return true;
        }
        charSet++;
    }
    return false;
}

char *FindFirstOf(const char *str, const char *charSet)
{
    if (str == NULL || charSet == NULL) {
        return NULL;
    }

    while (*str != '\0') {
        if (IsCharInSet(*str, charSet)) {
            return (char *)str;
        }
        str++;
    }
    return NULL;
}

char *FindLastOf(const char *str, const char *charSet)
{
    if (str == NULL || charSet == NULL) {
        return NULL;
    }

    const char *lastFound = NULL;
    while (*str != '\0') {
        if (IsCharInSet(*str, charSet)) {
            lastFound = str;
        }
        str++;
    }
    return (char *)lastFound;
}

size_t CountSubstring(const char *str, const char *subStr, bool overlap)
{
    if (str == NULL || subStr == NULL || *subStr == '\0') {
        return 0;
    }

    size_t count = 0;
    size_t subLen = strlen(subStr);
    const char *pos = str;

    while (*pos != '\0') {
        const char *found = strstr(pos, subStr);
        if (found == NULL) {
            break;
        }
        count++;
        if (overlap) {
            pos = found + 1;
        } else {
            pos = found + subLen;
        }
    }

    return count;
}

/* -------------------------------------------------------------------------- */
/* Advanced string parsing utilities                                           */
/* -------------------------------------------------------------------------- */

INIT_STATIC bool IsValidHexChar(char ch)
{
    return ((ch >= '0' && ch <= '9') ||
            (ch >= 'a' && ch <= 'f') ||
            (ch >= 'A' && ch <= 'F'));
}

INIT_STATIC int HexCharToValue(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    /* 字母 a-f 对应十六进制数值 10-15 */
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10; /* 10: 十六进制字母 'a' 的起始数值 */
    }
    /* 字母 A-F 对应十六进制数值 10-15 */
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10; /* 10: 十六进制字母 'A' 的起始数值 */
    }
    return -1;
}

INIT_STATIC bool IsValidOctalChar(char ch)
{
    return (ch >= '0' && ch <= '7');
}

INIT_STATIC int OctalCharToValue(char ch)
{
    if (ch >= '0' && ch <= '7') {
        return ch - '0';
    }
    return -1;
}

INIT_STATIC char *ReverseStringInPlace(char *str)
{
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str);
    if (len < STRING_HELPER_MIN_REVERSE_LEN) {
        return str;
    }

    size_t i = 0;
    size_t j = len - 1;

    while (i < j) {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }

    return str;
}

INIT_STATIC bool StringStartsWithChar(const char *str, char ch)
{
    if (str == NULL) {
        return false;
    }
    return (str[0] == ch);
}

INIT_STATIC bool StringEndsWithChar(const char *str, char ch)
{
    if (str == NULL) {
        return false;
    }
    size_t len = strlen(str);
    if (len == 0) {
        return false;
    }
    return (str[len - 1] == ch);
}
