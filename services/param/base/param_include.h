/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#ifndef BASE_STARTUP_PARAM_INCLUDE_H
#define BASE_STARTUP_PARAM_INCLUDE_H
#include <stdio.h>

#include "param_osadp.h"
#include "param_trie.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

STATIC_INLINE void GetNextKey(const char **remainingKey, char **subKey, uint32_t *subKeyLen, const char *end)
{
    *subKey = strchr(*remainingKey, '.');
    if (*subKey != NULL) {
        *subKeyLen = *subKey - *remainingKey;
    } else {
        *subKeyLen = end - *remainingKey;
    }
}

STATIC_INLINE ParamTrieNode *FindSubTrie(const WorkSpace *workSpace,
    ParamTrieNode *current, const char *key, uint32_t keyLen, uint32_t *matchLabel)
{
    ParamTrieNode *subTrie = current;
    int ret = 0;
    while (subTrie != NULL) {
        if (subTrie->length > keyLen) {
            ret = -1;
        } else if (subTrie->length < keyLen) {
            ret = 1;
        } else {
            ret = memcmp(subTrie->key, key, keyLen);
            if (ret == 0) {
                *matchLabel = (subTrie->labelIndex != 0) ? subTrie->labelIndex : *matchLabel;
                return subTrie;
            }
        }

        uint32_t offset = 0;
        if (ret < 0) {
            offset = subTrie->left;
        } else {
            offset = subTrie->right;
        }
        if (offset == 0 || offset > workSpace->area->dataSize) {
            return NULL;
        }
        subTrie = (ParamTrieNode *)(workSpace->area->data + offset);
    }
    return NULL;
}

STATIC_INLINE ParamTrieNode *FindTrieNode_(
    const WorkSpace *workSpace, const char *key, uint32_t keyLen, uint32_t *matchLabel)
{
    const char *remainingKey = key;
    ParamTrieNode *current = GetTrieRoot(workSpace);
    PARAM_CHECK(current != NULL, return NULL, "Invalid current param %s", key);
    *matchLabel = current->labelIndex;
    const char *end = key + keyLen;
    while (1) {
        uint32_t subKeyLen = 0;
        char *subKey = NULL;
        GetNextKey(&remainingKey, &subKey, &subKeyLen, end);
        if (!subKeyLen) {
            return NULL;
        }
        if (current->child != 0) {
            ParamTrieNode *next = GetTrieNode(workSpace, current->child);
            current = FindSubTrie(workSpace, next, remainingKey, subKeyLen, matchLabel);
        } else {
            current = FindSubTrie(workSpace, current, remainingKey, subKeyLen, matchLabel);
        }
        if (current == NULL) {
            return NULL;
        } else if (current->labelIndex != 0) {
            *matchLabel = current->labelIndex;
        }
        if (subKey == NULL || strcmp(subKey, ".") == 0) {
            break;
        }
        remainingKey = subKey + 1;
    }
    return current;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif  // BASE_STARTUP_PARAM_INCLUDE_H