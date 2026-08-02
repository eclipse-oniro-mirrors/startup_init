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

#ifndef STARTUP_INIT_STRING_HELPER_H
#define STARTUP_INIT_STRING_HELPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/* String buffer size constants */
#define STRING_HELPER_SMALL_BUF  64
#define STRING_HELPER_MEDIUM_BUF 256
#define STRING_HELPER_LARGE_BUF  1024
#define STRING_HELPER_MAX_BUF    4096

/* String matching flags */
#define STR_MATCH_CASE_SENSITIVE   0x01
#define STR_MATCH_WHOLE_WORD       0x02
#define STR_MATCH_FROM_START       0x04
#define STR_MATCH_FROM_END         0x08
#define STR_MATCH_IGNORE_WHITESPACE 0x10

/* Trim mode flags */
typedef enum {
    TRIM_MODE_LEFT       = 0x01,
    TRIM_MODE_RIGHT      = 0x02,
    TRIM_MODE_BOTH       = 0x03,
    TRIM_MODE_ALL        = 0x07,
    TRIM_MODE_COLLAPSE   = 0x0F
} TrimMode;

/* String split result context */
typedef struct StringSplitContext {
    char  *sourceStr;
    char  *delimiter;
    char  *currentPos;
    char  *savePtr;
    size_t tokenCount;
    size_t maxTokens;
    bool   skipEmpty;
} StringSplitContext;

/* Key-Value pair structure for string parsing */
typedef struct KeyValuePair {
    char *key;
    char *value;
    size_t keyLen;
    size_t valueLen;
} KeyValuePair;

/* String buffer with dynamic growth capability */
typedef struct DynamicStringBuffer {
    char   *data;
    size_t  capacity;
    size_t  length;
    size_t  growSize;
    bool    isOwned;
} DynamicStringBuffer;

/* Case conversion types */
typedef enum {
    CASE_LOWER        = 0,
    CASE_UPPER        = 1,
    CASE_TITLE        = 2,
    CASE_SENTENCE     = 3,
    CASE_TOGGLE       = 4,
    CASE_CAMEL        = 5,
    CASE_SNAKE_UPPER  = 6,
    CASE_SNAKE_LOWER  = 7
} CaseConversionType;

/* String validation result codes */
typedef enum {
    STR_VALID_OK              = 0,
    STR_VALID_NULL_INPUT      = -1,
    STR_VALID_EMPTY_STRING    = -2,
    STR_VALID_INVALID_CHAR    = -3,
    STR_VALID_TOO_LONG        = -4,
    STR_VALID_BUFFER_OVERFLOW = -5,
    STR_VALID_ENCODING_ERROR  = -6,
    STR_VALID_FORMAT_ERROR    = -7
} StringValidationResult;

/* String replacement options */
typedef struct StringReplaceOptions {
    const char *searchStr;
    const char *replaceStr;
    size_t      maxReplacements;
    bool        replaceAll;
    bool        ignoreCase;
    bool        useRegex;
} StringReplaceOptions;

/* -------------------------------------------------------------------------- */
/* Dynamic string buffer management                                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize a dynamic string buffer with initial capacity.
 * @param buf Pointer to DynamicStringBuffer structure.
 * @param initialCapacity Initial buffer capacity in bytes.
 * @param growSizeIncrement Size increment when buffer needs to grow.
 * @return 0 on success, -1 on failure.
 */
int InitDynamicStringBuffer(DynamicStringBuffer *buf, size_t initialCapacity, size_t growSizeIncrement);

/**
 * @brief Release resources held by a dynamic string buffer.
 * @param buf Pointer to DynamicStringBuffer structure.
 */
void ReleaseDynamicStringBuffer(DynamicStringBuffer *buf);

/**
 * @brief Append a string to the dynamic buffer, growing if necessary.
 * @param buf Pointer to DynamicStringBuffer structure.
 * @param src Source string to append.
 * @return 0 on success, -1 on failure.
 */
int DynamicStringBufferAppend(DynamicStringBuffer *buf, const char *src);

/**
 * @brief Append formatted string to the dynamic buffer.
 * @param buf Pointer to DynamicStringBuffer structure.
 * @param format Printf-style format string.
 * @param ... Variable arguments.
 * @return 0 on success, -1 on failure.
 */
int DynamicStringBufferAppendFormat(DynamicStringBuffer *buf, const char *format, ...);

/**
 * @brief Clear the contents of a dynamic string buffer without freeing memory.
 * @param buf Pointer to DynamicStringBuffer structure.
 */
void DynamicStringBufferClear(DynamicStringBuffer *buf);

/**
 * @brief Reserve additional capacity in the dynamic string buffer.
 * @param buf Pointer to DynamicStringBuffer structure.
 * @param additionalSize Additional bytes to reserve.
 * @return 0 on success, -1 on failure.
 */
int DynamicStringBufferReserve(DynamicStringBuffer *buf, size_t additionalSize);

/**
 * @brief Shrink the buffer capacity to match its current length.
 * @param buf Pointer to DynamicStringBuffer structure.
 * @return 0 on success, -1 on failure.
 */
int DynamicStringBufferShrinkToFit(DynamicStringBuffer *buf);

/* -------------------------------------------------------------------------- */
/* String trimming and whitespace                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Trim whitespace characters from a string in-place.
 * @param str String to trim (modified in place).
 * @param mode Trim mode specifying which sides to trim.
 * @return Pointer to the trimmed string (same as input).
 */
char *TrimString(char *str, TrimMode mode);

/**
 * @brief Create a trimmed copy of a string.
 * @param str Source string to trim.
 * @param mode Trim mode specifying which sides to trim.
 * @return Newly allocated trimmed string, or NULL on failure. Caller must free.
 */
char *TrimStringCopy(const char *str, TrimMode mode);

/**
 * @brief Check if a string consists only of whitespace characters.
 * @param str String to check.
 * @return true if string is all whitespace or empty, false otherwise.
 */
bool IsStringBlank(const char *str);

/**
 * @brief Count the number of non-whitespace characters in a string.
 * @param str String to count.
 * @return Number of non-whitespace characters.
 */
size_t CountNonWhitespace(const char *str);

/**
 * @brief Remove all whitespace from a string in-place.
 * @param str String to compact.
 * @return Pointer to the compacted string.
 */
char *RemoveAllWhitespace(char *str);

/**
 * @brief Collapse multiple consecutive whitespace characters into a single space.
 * @param str String to normalize.
 * @return Pointer to the normalized string.
 */
char *CollapseWhitespace(char *str);

/* -------------------------------------------------------------------------- */
/* String comparison and searching                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Compare two strings with configurable matching options.
 * @param str1 First string to compare.
 * @param str2 Second string to compare.
 * @param flags Bitmask of STR_MATCH_* flags.
 * @return 0 if strings match according to flags, non-zero otherwise.
 */
int CompareStringsWithFlags(const char *str1, const char *str2, uint32_t flags);

/**
 * @brief Find the first occurrence of any character from a set.
 * @param str String to search in.
 * @param charSet Set of characters to search for.
 * @return Pointer to first matching character, or NULL if not found.
 */
char *FindFirstOf(const char *str, const char *charSet);

/**
 * @brief Find the last occurrence of any character from a set.
 * @param str String to search in.
 * @param charSet Set of characters to search for.
 * @return Pointer to last matching character, or NULL if not found.
 */
char *FindLastOf(const char *str, const char *charSet);

/**
 * @brief Count occurrences of a substring within a string.
 * @param str String to search in.
 * @param subStr Substring to count.
 * @param overlap Whether to count overlapping occurrences.
 * @return Number of occurrences found.
 */
size_t CountSubstring(const char *str, const char *subStr, bool overlap);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* STARTUP_INIT_STRING_HELPER_H */
