/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

#include "param_security.h"
#include "securec.h"
#include "sys_param.h"

#ifndef __NR_futex
#define PARAM_NR_FUTEX 202 /* syscall number */
#else
#define PARAM_NR_FUTEX __NR_futex
#endif

#if defined FUTEX_WAIT || defined FUTEX_WAKE
#include <linux/futex.h>
#else
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

#define PARAM_FUTEX(ftx, op, value, timeout, bitset)                         \
    do {                                                                   \
        struct timespec d_timeout = { 0, 1000 * 1000 * (timeout) };        \
        syscall(PARAM_NR_FUTEX, ftx, op, value, &d_timeout, NULL, bitset); \
    } while (0)

#define futex_wake(ftx, count) PARAM_FUTEX(ftx, FUTEX_WAKE, count, 0, 0)
#define futex_wait(ftx, value) PARAM_FUTEX(ftx, FUTEX_WAIT, value, 100, 0)
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define PARAM_WORKSPACE_MAX (80 * 1024)
#define FILENAME_LEN_MAX 255
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

typedef struct {
    atomic_uint commitId;
    uint16_t keyLength;
    uint16_t valueLength;
    char data[0];
} ParamNode;

typedef struct {
    uid_t uid;
    gid_t gid;
    uint16_t mode;
    uint16_t length;
    char data[0];
} ParamSecruityNode;

typedef struct {
    uint32_t trieNodeCount;
    uint32_t paramNodeCount;
    uint32_t securityNodeCount;
    uint32_t currOffset;
    uint32_t firstNode;
    uint32_t dataSize;
    uint32_t reserved_[28];
    char data[0];
} ParamTrieHeader;

struct WorkSpace_;
typedef struct WorkSpace_ {
    char fileName[FILENAME_LEN_MAX + 1];
    uint32_t (*allocTrieNode)(struct WorkSpace_ *workSpace, const char *key, uint32_t keyLen);
    int (*compareTrieNode)(const ParamTrieNode *node, const char *key2, uint32_t key2Len);
    ParamTrieHeader *area;
} WorkSpace;

int InitWorkSpace(const char *fileName, WorkSpace *workSpace, int onlyRead);
void CloseWorkSpace(WorkSpace *workSpace);

ParamTrieNode *GetTrieNode(const WorkSpace *workSpace, uint32_t offset);
void SaveIndex(uint32_t *index, uint32_t offset);

ParamTrieNode *AddTrieNode(WorkSpace *workSpace, const char *key, uint32_t keyLen);
ParamTrieNode *FindTrieNode(const WorkSpace *workSpace, const char *key, uint32_t keyLen, uint32_t *matchLabel);

typedef int (*TraversalTrieNodePtr)(const WorkSpace *workSpace, const ParamTrieNode *node, void *cookie);
int TraversalTrieNode(const WorkSpace *workSpace, const ParamTrieNode *subTrie, TraversalTrieNodePtr walkFunc, void *cookie);

uint32_t AddParamSecruityNode(WorkSpace *workSpace, const ParamAuditData *auditData);
uint32_t AddParamNode(WorkSpace *workSpace, const char *key, uint32_t keyLen, const char *value, uint32_t valueLen);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // BASE_STARTUP_PARAM_TRIE_H