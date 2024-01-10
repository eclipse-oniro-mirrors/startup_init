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

#ifndef BASE_STARTUP_PARAM_TRIE_H
#define BASE_STARTUP_PARAM_TRIE_H
#include <stdio.h>

#include "init_hashmap.h"
#include "init_param.h"
#include "list.h"
#include "param_common.h"
#include "param_osadp.h"
#include "param_security.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PARAM_TRIE_STACK_SIZE 25

INIT_LOCAL_API int InitWorkSpace(WorkSpace *workSpace, int onlyRead, uint32_t spaceSize);
INIT_LOCAL_API void CloseWorkSpace(WorkSpace *workSpace);

#define GetTrieNode(workSpace, offset) (ParamTrieNode *)(((offset) == 0 || (offset) > (workSpace)->area->dataSize) ? \
    NULL : (workSpace)->area->data + (offset))

#define GetTrieRoot(workSpace) \
    (ParamTrieNode *)(((workSpace)->area == NULL) ? NULL : (workSpace)->area->data + (workSpace)->area->firstNode)

INIT_LOCAL_API void SaveIndex(uint32_t *index, uint32_t offset);

INIT_LOCAL_API ParamTrieNode *AddTrieNode(WorkSpace *workSpace, const char *key, uint32_t keyLen);
INIT_LOCAL_API ParamTrieNode *FindTrieNode(
    WorkSpace *workSpace, const char *key, uint32_t keyLen, uint32_t *matchLabel);

typedef int (*TraversalTrieNodePtr)(const WorkSpace *workSpace, const ParamTrieNode *node, const void *cookie);
INIT_LOCAL_API int TraversalTrieNode(const WorkSpace *workSpace,
    const ParamTrieNode *subTrie, TraversalTrieNodePtr walkFunc, const void *cookie);

INIT_LOCAL_API uint32_t AddParamSecurityNode(WorkSpace *workSpace, const ParamAuditData *auditData);
INIT_LOCAL_API uint32_t AddParamNode(WorkSpace *workSpace, uint8_t type,
    const char *key, uint32_t keyLen, const char *value, uint32_t valueLen);

INIT_LOCAL_API uint32_t GetParamMaxLen(uint8_t type);
INIT_LOCAL_API ParamNode *GetParamNode(uint32_t index, const char *name);
INIT_LOCAL_API int AddParamEntry(uint32_t index, uint8_t type, const char *name, const char *value);

#ifdef STARTUP_INIT_TEST
STATIC_INLINE ParamTrieNode *FindTrieNode_(
    const WorkSpace *workSpace, const char *key, uint32_t keyLen, uint32_t *matchLabel);
#endif

#define GetWorkSpaceSize(workSpace)     \
    ((workSpace == NULL) || (workSpace->area == NULL) ||     \
        (workSpace->area->spaceSizeOffset == 0) ||    \
        (workSpace->area->spaceSizeOffset >= workSpace->area->dataSize)) ? NULL : \
        (WorkSpaceSize*)(workSpace->area->data + workSpace->area->spaceSizeOffset)

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif  // BASE_STARTUP_PARAM_TRIE_H