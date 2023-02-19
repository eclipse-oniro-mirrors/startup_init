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
#include "init_hashmap.h"
#include "init_log.h"

typedef struct {
    HashNodeCompare nodeCompare;
    HashKeyCompare keyCompare;
    HashNodeFunction nodeHash;
    HashKeyFunction keyHash;
    HashNodeOnFree nodeFree;
    int maxBucket;
    uint32_t tableId;
    HashNode *buckets[0];
} HashTab;

static uint32_t g_tableId = 0;
int32_t OH_HashMapCreate(HashMapHandle *handle, const HashInfo *info)
{
    INIT_ERROR_CHECK(handle != NULL && info != NULL && info->maxBucket > 0, return -1, "Invalid param");
    INIT_ERROR_CHECK(info->keyHash != NULL && info->nodeHash != NULL, return -1, "Invalid param");
    INIT_ERROR_CHECK(info->nodeCompare != NULL && info->keyCompare != NULL, return -1, "Invalid param");
    HashTab *tab = (HashTab *)calloc(1, sizeof(HashTab) + sizeof(HashNode*) * info->maxBucket);
    INIT_ERROR_CHECK(tab != NULL, return -1, "Failed to create hash tab");
    tab->maxBucket = info->maxBucket;
    tab->keyHash = info->keyHash;
    tab->nodeCompare = info->nodeCompare;
    tab->keyCompare = info->keyCompare;
    tab->nodeHash = info->nodeHash;
    tab->nodeFree = info->nodeFree;
    tab->tableId = g_tableId++;
    *handle = (HashMapHandle)tab;
    return 0;
}

static HashNode *GetHashNodeByNode(const HashTab *tab, const HashNode *root, const HashNode *new)
{
    HashNode *node = (HashNode *)root;
    while (node != NULL) {
        int ret = tab->nodeCompare(node, new);
        if (ret == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

static HashNode *GetHashNodeByKey(const HashTab *tab, const HashNode *root, const void *key, HashKeyCompare keyCompare)
{
    (void)tab;
    HashNode *node = (HashNode *)root;
    while (node != NULL) {
        int ret = keyCompare(node, key);
        if (ret == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

int32_t OH_HashMapAdd(HashMapHandle handle, HashNode *node)
{
    INIT_ERROR_CHECK(handle != NULL, return -1, "Invalid param");
    INIT_ERROR_CHECK(node != NULL && node->next == NULL, return -1, "Invalid param");
    HashTab *tab = (HashTab *)handle;
    int hashCode = tab->nodeHash(node);
    hashCode = (hashCode < 0) ? -hashCode : hashCode;
    hashCode = hashCode % tab->maxBucket;
    INIT_ERROR_CHECK(hashCode < tab->maxBucket, return -1, "Invalid hashcode %d %d", tab->maxBucket, hashCode);

    // check key exist
    HashNode *tmp = GetHashNodeByNode(tab, tab->buckets[hashCode], node);
    if (tmp != NULL) {
        INIT_LOGE("node hash been exist");
        return -1;
    }
    node->next = tab->buckets[hashCode];
    tab->buckets[hashCode] = node;
    INIT_LOGV("OH_HashMapAdd tableId %d hashCode %d", tab->tableId, hashCode);
    return 0;
}

void OH_HashMapRemove(HashMapHandle handle, const void *key)
{
    INIT_ERROR_CHECK(handle != NULL && key != NULL, return, "Invalid param");
    HashTab *tab = (HashTab *)handle;
    int hashCode = tab->keyHash(key);
    hashCode = (hashCode < 0) ? -hashCode : hashCode;
    hashCode = hashCode % tab->maxBucket;
    INIT_ERROR_CHECK(hashCode < tab->maxBucket, return, "Invalid hashcode %d %d", tab->maxBucket, hashCode);

    HashNode *node = tab->buckets[hashCode];
    HashNode *preNode = node;
    while (node != NULL) {
        int ret = tab->keyCompare(node, key);
        if (ret == 0) {
            if (node == tab->buckets[hashCode]) {
                tab->buckets[hashCode] = node->next;
            } else {
                preNode->next = node->next;
            }
            return;
        }
        preNode = node;
        node = node->next;
    }
}

HashNode *OH_HashMapGet(HashMapHandle handle, const void *key)
{
    INIT_ERROR_CHECK(handle != NULL && key != NULL, return NULL, "Invalid param %s", key);
    HashTab *tab = (HashTab *)handle;
    int hashCode = tab->keyHash(key);
    hashCode = (hashCode < 0) ? -hashCode : hashCode;
    hashCode = hashCode % tab->maxBucket;
    INIT_ERROR_CHECK(hashCode < tab->maxBucket, return NULL,
        "Invalid hashcode %d %d", tab->maxBucket, hashCode);
    return GetHashNodeByKey(tab, tab->buckets[hashCode], key, tab->keyCompare);
}

static void HashListFree(HashTab *tab, HashNode *root)
{
    if (root == NULL) {
        return;
    }
    HashNode *node = root;
    while (node != NULL) {
        HashNode *next = node->next;
        if (tab->nodeFree != NULL) {
            tab->nodeFree(node);
        }
        node = next;
    }
}

void OH_HashMapDestory(HashMapHandle handle)
{
    INIT_ERROR_CHECK(handle != NULL, return, "Invalid param");
    HashTab *tab = (HashTab *)handle;
    for (int i = 0; i < tab->maxBucket; i++) {
        HashListFree(tab, tab->buckets[i]);
    }
    free(tab);
}

HashNode *OH_HashMapFind(HashMapHandle handle,
    int hashCode, const void *key, HashKeyCompare keyCompare)
{
    INIT_ERROR_CHECK(handle != NULL && key != NULL, return NULL, "Invalid param");
    INIT_ERROR_CHECK(handle != NULL && keyCompare != NULL, return NULL, "Invalid param");
    HashTab *tab = (HashTab *)handle;
    INIT_ERROR_CHECK(hashCode < tab->maxBucket, return NULL,
        "Invalid hashcode %d %d", tab->maxBucket, hashCode);
    return GetHashNodeByKey(tab, tab->buckets[hashCode], key, keyCompare);
}

void OH_HashMapTraverse(HashMapHandle handle, void (*hashNodeTraverse)(const HashNode *node, const void *context),
    const void *context)
{
    INIT_ERROR_CHECK(handle != NULL && hashNodeTraverse != NULL, return, "Invalid param");
    HashTab *tab = (HashTab *)handle;
    for (int i = 0; i < tab->maxBucket; i++) {
        HashNode *node = tab->buckets[i];
        while (node != NULL) {
            HashNode *next = node->next;
            hashNodeTraverse(node, context);
            node = next;
        }
    }
}

int OH_HashMapIsEmpty(HashMapHandle handle)
{
    INIT_ERROR_CHECK(handle != NULL, return 1, "Invalid param");
    HashTab *tab = (HashTab *)handle;
    for (int i = 0; i < tab->maxBucket; i++) {
        HashNode *node = tab->buckets[i];
        if (node != NULL) {
            return 0;
        }
    }
    return 1;
}
