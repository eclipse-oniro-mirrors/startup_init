/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "param_osadp.h"
#include "param_security.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifdef PARAM_SUPPORT_SELINUX
#define HASH_BUTT 32
#else
#define HASH_BUTT 1
#endif

typedef struct {
    uint32_t left;
    uint32_t right;
    uint32_t child;
    uint32_t labelIndex;
    uint32_t dataIndex;
    uint16_t length;
    char key[0];
} ParamTrieNode;

#define PARAM_FLAGS_MODIFY 0x80000000
#define PARAM_FLAGS_TRIGGED 0x40000000
#define PARAM_FLAGS_WAITED 0x20000000
#define PARAM_FLAGS_COMMITID 0x0000ffff

#define PARAM_TYPE_MASK   0x0f
#define PARAM_TYPE_STRING 0x00
#define PARAM_TYPE_INT    0x01
#define PARAM_TYPE_BOOL   0x02

typedef struct {
    ATOMIC_UINT32 commitId;
    uint8_t type;
    uint8_t keyLength;
    uint16_t valueLength;
    char data[0];
} ParamNode;

typedef struct {
    uid_t uid;
    gid_t gid;
    uint16_t mode;
    uint8_t type;
    uint8_t length;
    char data[0];
} ParamSecurityNode;

typedef struct {
    ATOMIC_LLONG commitId;
    ATOMIC_LLONG commitPersistId;
    uint32_t trieNodeCount;
    uint32_t paramNodeCount;
    uint32_t securityNodeCount;
    uint32_t startIndex;
    uint32_t currOffset;
    uint32_t firstNode;
    uint32_t dataSize;
    char data[0];
} ParamTrieHeader;

struct WorkSpace_;
typedef struct WorkSpace_ {
    unsigned int flags;
    HashNode hashNode;
    ListNode node;
    uint32_t (*allocTrieNode)(struct WorkSpace_ *workSpace, const char *key, uint32_t keyLen);
    int (*compareTrieNode)(const ParamTrieNode *node, const char *key2, uint32_t key2Len);
    MemHandle memHandle;
    ParamTrieHeader *area;
#ifdef WORKSPACE_AREA_NEED_MUTEX
    ParamRWMutex rwlock;
#endif
    char fileName[0];
} WorkSpace;

INIT_LOCAL_API int InitWorkSpace(WorkSpace *workSpace, int onlyRead, uint32_t spaceSize);
INIT_LOCAL_API void CloseWorkSpace(WorkSpace *workSpace);

INIT_LOCAL_API ParamTrieNode *GetTrieNode(const WorkSpace *workSpace, uint32_t offset);
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
INIT_LOCAL_API ParamNode *GetParamNode(const char *spaceName, const char *name);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif  // BASE_STARTUP_PARAM_TRIE_H