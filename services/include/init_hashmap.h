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
#ifndef INIT_HASH_MAP_
#define INIT_HASH_MAP_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "init_log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define HASH_TAB_BUCKET_MAX 1024
#define HASH_TAB_BUCKET_MIN 16

typedef struct HashNode_ {
    struct HashNode_ *next;
} HashNode;

#define HASHMAP_ENTRY(ptr, type, member)   ((type *)((char *)(ptr) - offsetof(type, member)))
#define HASHMAPInitNode(node) (node)->next = NULL

typedef int (*HashNodeCompare)(const HashNode *node1, const HashNode *node2);
typedef int (*HashKeyCompare)(const HashNode *node1, const void *key);
typedef int (*HashNodeFunction)(const HashNode *node);
typedef int (*HashKeyFunction)(const void *key);
typedef void (*HashNodeOnFree)(const HashNode *node);

typedef struct {
    HashNodeCompare nodeCompare;
    HashKeyCompare keyCompare;
    HashNodeFunction nodeHash;
    HashKeyFunction keyHash;
    HashNodeOnFree nodeFree;
    int maxBucket;
} HashInfo;

typedef void *HashMapHandle;

int32_t HashMapCreate(HashMapHandle *handle, const HashInfo *info);
void HashMapDestory(HashMapHandle handle);
int32_t HashMapAdd(HashMapHandle handle, HashNode *hashNode);
void HashMapRemove(HashMapHandle handle, const void *key);
HashNode *HashMapGet(HashMapHandle handle, const void *key);
HashNode *HashMapFind(HashMapHandle handle,
    int hashCode, const void *key, HashKeyCompare keyCompare);
void HashMapTraverse(HashMapHandle handle, void (*hashNodeTraverse)(const HashNode *node, const void *context),
    const void *context);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif