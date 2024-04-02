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

#include "param_trie.h"

#include <errno.h>

#include "init_param.h"
#include "param_base.h"
#include "param_manager.h"
#include "param_osadp.h"
#include "param_utils.h"
#include "param_include.h"

#define OFFSET_ERR 0

static uint32_t AllocateParamTrieNode(WorkSpace *workSpace, const char *key, uint32_t keyLen);

static int GetRealFileName(WorkSpace *workSpace, char *buffer, uint32_t size)
{
    int ret = PARAM_SPRINTF(buffer, size, "%s/%s", PARAM_STORAGE_PATH, workSpace->fileName);
    PARAM_CHECK(ret > 0, return -1, "Failed to copy file name %s", workSpace->fileName);
    buffer[ret] = '\0';
    return 0;
}

static int InitWorkSpace_(WorkSpace *workSpace, uint32_t spaceSize, int readOnly)
{
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    PARAM_CHECK(sizeof(ParamTrieHeader) < spaceSize,
        return PARAM_CODE_INVALID_PARAM, "Invalid spaceSize %u name %s", spaceSize, workSpace->fileName);

    char buffer[FILENAME_LEN_MAX] = {0};
    int ret = GetRealFileName(workSpace, buffer, sizeof(buffer));
    PARAM_CHECK(ret == 0, return -1, "Failed to get file name %s", workSpace->fileName);
    void *areaAddr = GetSharedMem(buffer, &workSpace->memHandle, spaceSize, readOnly);
    PARAM_ONLY_CHECK(areaAddr != NULL, return PARAM_CODE_MEMORY_MAP_FAILED);
    if (!readOnly) {
        workSpace->area = (ParamTrieHeader *)areaAddr;
        ATOMIC_UINT64_INIT(&workSpace->area->commitId, 0);
        ATOMIC_UINT64_INIT(&workSpace->area->commitPersistId, 0);
        workSpace->area->trieNodeCount = 0;
        workSpace->area->paramNodeCount = 0;
        workSpace->area->securityNodeCount = 0;
        workSpace->area->dataSize = spaceSize - sizeof(ParamTrieHeader);
        workSpace->area->spaceSizeOffset = 0;
        workSpace->area->currOffset = 0;
        uint32_t offset = AllocateParamTrieNode(workSpace, "#", 1);
        workSpace->area->firstNode = offset;
    } else {
        workSpace->area = (ParamTrieHeader *)areaAddr;
    }
    PARAM_LOGV("InitWorkSpace success, readOnly %d currOffset %u firstNode %u dataSize %u",
        readOnly, workSpace->area->currOffset, workSpace->area->firstNode, workSpace->area->dataSize);
    return 0;
}

static uint32_t AllocateParamTrieNode(WorkSpace *workSpace, const char *key, uint32_t keyLen)
{
    uint32_t len = keyLen + sizeof(ParamTrieNode) + 1;
    len = PARAM_ALIGN(len);
    PARAM_CHECK((workSpace->area->currOffset + len) < workSpace->area->dataSize, return 0,
        "Failed to allocate currOffset %d, dataSize %d space %s",
        workSpace->area->currOffset, workSpace->area->dataSize, workSpace->fileName);
    ParamTrieNode *node = (ParamTrieNode *)(workSpace->area->data + workSpace->area->currOffset);
    node->length = keyLen;
    int ret = PARAM_MEMCPY(node->key, keyLen, key, keyLen);
    PARAM_CHECK(ret == 0, return 0, "Failed to copy key");
    node->key[keyLen] = '\0';
    node->left = 0;
    node->right = 0;
    node->child = 0;
    node->dataIndex = 0;
    node->labelIndex = 0;
    uint32_t offset = workSpace->area->currOffset;
    workSpace->area->currOffset += len;
    workSpace->area->trieNodeCount++;
    return offset;
}

INIT_LOCAL_API int InitWorkSpace(WorkSpace *workSpace, int onlyRead, uint32_t spaceSize)
{
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_NAME, "Invalid workSpace");
    if (PARAM_TEST_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    int ret = InitWorkSpace_(workSpace, spaceSize, onlyRead);
    PARAM_ONLY_CHECK(ret == 0, return ret);
    PARAM_SET_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT);
    PARAM_LOGV("InitWorkSpace %s for %s", workSpace->fileName, (onlyRead == 0) ? "init" : "other");
    return ret;
}

INIT_LOCAL_API void CloseWorkSpace(WorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL, return, "The workspace is null");
    PARAM_LOGV("CloseWorkSpace %s", workSpace->fileName);
    if (!PARAM_TEST_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT)) {
        return;
    }
    PARAM_CHECK(workSpace->area != NULL, return, "The workspace area is null");
#ifdef WORKSPACE_AREA_NEED_MUTEX
    ParamRWMutexDelete(&workSpace->rwlock);
#endif
    FreeSharedMem(&workSpace->memHandle, workSpace->area, workSpace->area->dataSize);
    workSpace->area = NULL;
}

static int CheckWorkSpace(const WorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return PARAM_WORKSPACE_NOT_INIT, "The workspace is null");
    if (!PARAM_TEST_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT)) {
        return PARAM_WORKSPACE_NOT_INIT;
    }
    return 0;
}

static int CompareParamTrieNode(const ParamTrieNode *node, const char *key, uint32_t keyLen)
{
    if (node->length > keyLen) {
        return -1;
    } else if (node->length < keyLen) {
        return 1;
    }
    return memcmp(node->key, key, keyLen);
}

static ParamTrieNode *AddToSubTrie(WorkSpace *workSpace, ParamTrieNode *current, const char *key, uint32_t keyLen)
{
    if (current == NULL) {
        return NULL;
    }
    ParamTrieNode *subTrie = NULL;
    int ret = CompareParamTrieNode(current, key, keyLen);
    if (ret == 0) {
        return current;
    }
    if (ret < 0) {
        subTrie = GetTrieNode(workSpace, current->left);
        if (subTrie == NULL) {
            uint32_t offset = AllocateParamTrieNode(workSpace, key, keyLen);
            PARAM_CHECK(offset != 0, return NULL,
                "Failed to allocate key '%s' in space '%s'", key, workSpace->fileName);
            SaveIndex(&current->left, offset);
            return GetTrieNode(workSpace, current->left);
        }
    } else {
        subTrie = GetTrieNode(workSpace, current->right);
        if (subTrie == NULL) {
            uint32_t offset = AllocateParamTrieNode(workSpace, key, keyLen);
            PARAM_CHECK(offset != 0, return NULL,
                "Failed to allocate key '%s' in space '%s'", key, workSpace->fileName);
            SaveIndex(&current->right, offset);
            return GetTrieNode(workSpace, current->right);
        }
    }
    return AddToSubTrie(workSpace, subTrie, key, keyLen);
}

ParamTrieNode *AddTrieNode(WorkSpace *workSpace, const char *key, uint32_t keyLen)
{
    PARAM_CHECK(key != NULL && keyLen > 0, return NULL, "Invalid param ");
    PARAM_CHECK(CheckWorkSpace(workSpace) == 0, return NULL, "Invalid workSpace %s", key);
    const char *remainingKey = key;
    ParamTrieNode *current = GetTrieRoot(workSpace);
    PARAM_CHECK(current != NULL, return NULL, "Invalid current param %s", key);
    while (1) {
        uint32_t subKeyLen = 0;
        char *subKey = NULL;
        GetNextKey(&remainingKey, &subKey, &subKeyLen, key + keyLen);
        if (!subKeyLen) {
            return NULL;
        }
        if (current->child != 0) {  // 如果child存在，则检查是否匹配
            ParamTrieNode *next = GetTrieNode(workSpace, current->child);
            current = AddToSubTrie(workSpace, next, remainingKey, subKeyLen);
        } else {
            uint32_t dataOffset = AllocateParamTrieNode(workSpace, remainingKey, subKeyLen);
            PARAM_CHECK(dataOffset != 0, return NULL,
                "Failed to allocate key '%s' in space '%s'", key, workSpace->fileName);
            SaveIndex(&current->child, dataOffset);
            current = (ParamTrieNode *)GetTrieNode(workSpace, current->child);
        }
        if (current == NULL) {
            return NULL;
        }
        if (subKey == NULL || strcmp(subKey, ".") == 0) {
            break;
        }
        remainingKey = subKey + 1;
    }
    return current;
}

static int TraversalSubTrieNode(const WorkSpace *workSpace,
    const ParamTrieNode *current, TraversalTrieNodePtr walkFunc, const void *cookie)
{
    if (current == NULL) {
        return 0;
    }

    walkFunc(workSpace, (ParamTrieNode *)current, cookie);
    TraversalSubTrieNode(workSpace, GetTrieNode(workSpace, current->child), walkFunc, cookie);
    TraversalSubTrieNode(workSpace, GetTrieNode(workSpace, current->left), walkFunc, cookie);
    TraversalSubTrieNode(workSpace, GetTrieNode(workSpace, current->right), walkFunc, cookie);
    return 0;
}

INIT_LOCAL_API int TraversalTrieNode(const WorkSpace *workSpace,
    const ParamTrieNode *root, TraversalTrieNodePtr walkFunc, const void *cookie)
{
    PARAM_CHECK(walkFunc != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    PARAM_CHECK(CheckWorkSpace(workSpace) == 0, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    ParamTrieNode *current = (ParamTrieNode *)root;
    if (root == NULL) {
        current = GetTrieRoot(workSpace);
    }
    PARAM_CHECK(current != NULL, return 0, "Invalid current node");
    walkFunc(workSpace, (ParamTrieNode *)current, cookie);
    TraversalSubTrieNode(workSpace, GetTrieNode(workSpace, current->child), walkFunc, cookie);
    if (root == NULL) {
        TraversalSubTrieNode(workSpace, GetTrieNode(workSpace, current->left), walkFunc, cookie);
        TraversalSubTrieNode(workSpace, GetTrieNode(workSpace, current->right), walkFunc, cookie);
    }
    return 0;
}

INIT_LOCAL_API uint32_t AddParamSecurityNode(WorkSpace *workSpace, const ParamAuditData *auditData)
{
    PARAM_CHECK(CheckWorkSpace(workSpace) == 0, return OFFSET_ERR, "Invalid workSpace");
    PARAM_CHECK(auditData != NULL, return OFFSET_ERR, "Invalid auditData");
    uint32_t realLen = PARAM_ALIGN(sizeof(ParamSecurityNode) + sizeof(uid_t) * auditData->memberNum);
    PARAM_CHECK((workSpace->area->currOffset + realLen) < workSpace->area->dataSize,
        return OFFSET_ERR, "Failed to allocate currOffset %u, dataSize %u datalen %u",
        workSpace->area->currOffset, workSpace->area->dataSize, realLen);
    ParamSecurityNode *node = (ParamSecurityNode *)(workSpace->area->data + workSpace->area->currOffset);
    node->uid = auditData->dacData.uid;
    node->gid = auditData->dacData.gid;
    node->mode = auditData->dacData.mode;
    node->type = auditData->dacData.paramType & PARAM_TYPE_MASK;
#ifdef PARAM_SUPPORT_SELINUX
    node->selinuxIndex = auditData->selinuxIndex;
#else
    node->selinuxIndex = 0;
#endif
    if (auditData->memberNum > 0) {
        // copy member
        int ret = PARAM_MEMCPY(node->members,
            realLen - sizeof(ParamSecurityNode), auditData->members, auditData->memberNum * sizeof(uid_t));
        PARAM_CHECK(ret == 0, return OFFSET_ERR, "Failed to copy members");
    }
    node->memberNum = auditData->memberNum;
    uint32_t offset = workSpace->area->currOffset;
    workSpace->area->currOffset += realLen;
    workSpace->area->securityNodeCount++;
    return offset;
}

INIT_LOCAL_API uint32_t AddParamNode(WorkSpace *workSpace, uint8_t type,
    const char *key, uint32_t keyLen, const char *value, uint32_t valueLen)
{
    PARAM_CHECK(key != NULL && value != NULL, return OFFSET_ERR, "Invalid param");
    PARAM_CHECK(CheckWorkSpace(workSpace) == 0, return OFFSET_ERR, "Invalid workSpace %s", key);

    uint32_t realLen = sizeof(ParamNode) + 1 + 1;
    // for const parameter, alloc memory on demand
    if ((valueLen > PARAM_VALUE_LEN_MAX) || IS_READY_ONLY(key)) {
        realLen += keyLen + valueLen;
    } else {
        realLen += keyLen + GetParamMaxLen(type);
    }
    realLen = PARAM_ALIGN(realLen);
    PARAM_CHECK((workSpace->area->currOffset + realLen) < workSpace->area->dataSize,
        return OFFSET_ERR, "Failed to allocate currOffset %u, dataSize %u datalen %u",
        workSpace->area->currOffset, workSpace->area->dataSize, realLen);

    ParamNode *node = (ParamNode *)(workSpace->area->data + workSpace->area->currOffset);
    ATOMIC_INIT(&node->commitId, 0);

    node->type = type;
    node->keyLength = keyLen;
    node->valueLength = valueLen;
    int ret = PARAM_SPRINTF(node->data, realLen, "%s=%s", key, value);
    PARAM_CHECK(ret > 0, return OFFSET_ERR, "Failed to sprint key and value");
    uint32_t offset = workSpace->area->currOffset;
    workSpace->area->currOffset += realLen;
    workSpace->area->paramNodeCount++;
    return offset;
}

INIT_LOCAL_API void SaveIndex(uint32_t *index, uint32_t offset)
{
    *index = offset;
}

INIT_LOCAL_API ParamTrieNode *FindTrieNode(WorkSpace *workSpace,
    const char *key, uint32_t keyLen, uint32_t *matchLabel)
{
    PARAM_ONLY_CHECK(key != NULL && keyLen > 0, return NULL);
    PARAM_CHECK(workSpace != NULL, return NULL, "Invalid workspace for %s", key);
    uint32_t tmpMatchLen = 0;
    ParamTrieNode *node = NULL;
    PARAMSPACE_AREA_RD_LOCK(workSpace);
    node = FindTrieNode_(workSpace, key, keyLen, &tmpMatchLen);
    PARAMSPACE_AREA_RW_UNLOCK(workSpace);
    if (matchLabel != NULL) {
        *matchLabel = tmpMatchLen;
    }
    if (node != NULL && node->dataIndex != 0) {
        ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, node->dataIndex);
        if (entry != NULL && entry->keyLength == keyLen) {
            return node;
        }
        return NULL;
    }
    return node;
}

INIT_LOCAL_API uint32_t GetParamMaxLen(uint8_t type)
{
    static const uint32_t typeLengths[] = {
        PARAM_VALUE_LEN_MAX, 32, 8 // 8 max bool length 32 max int length
    };
    if (type >= ARRAY_LENGTH(typeLengths)) {
        return PARAM_VALUE_LEN_MAX;
    }
    return typeLengths[type];
}

INIT_LOCAL_API ParamNode *GetParamNode(uint32_t index, const char *name)
{
    uint32_t labelIndex = 0;
    WorkSpace *space = GetWorkSpace(index);
    ParamTrieNode *entry = FindTrieNode(space, name, strlen(name), &labelIndex);
    if (entry == NULL || entry->dataIndex == 0) {
        return NULL;
    }
    return (ParamNode *)GetTrieNode(space, entry->dataIndex);
}

INIT_LOCAL_API int AddParamEntry(uint32_t index, uint8_t type, const char *name, const char *value)
{
    WorkSpace *workSpace = GetWorkSpace(WORKSPACE_INDEX_BASE);
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_ERROR, "Invalid workspace");
    ParamTrieNode *node = FindTrieNode(workSpace, name, strlen(name), NULL);
    if (node != NULL && node->dataIndex != 0) {
        return 0;
    }
    node = AddTrieNode(workSpace, name, strlen(name));
    PARAM_CHECK(node != NULL, return PARAM_CODE_REACHED_MAX, "Failed to add node");
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, node->dataIndex);
    if (entry == NULL) {
        uint32_t offset = AddParamNode(workSpace, type, name, strlen(name), value, strlen(value));
        PARAM_CHECK(offset > 0, return PARAM_CODE_REACHED_MAX, "Failed to allocate name %s", name);
        SaveIndex(&node->dataIndex, offset);
    }
    return 0;
}

INIT_LOCAL_API int AddSecurityLabel(const ParamAuditData *auditData)
{
    PARAM_CHECK(auditData != NULL && auditData->name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid auditData");
    WorkSpace *workSpace = GetWorkSpace(WORKSPACE_INDEX_DAC);
    PARAM_CHECK(workSpace != NULL, return PARAM_WORKSPACE_NOT_INIT, "Invalid workSpace");
    ParamTrieNode *node = GetTrieRoot(workSpace);
    if ((node == NULL) || (CompareParamTrieNode(node, auditData->name, strlen(auditData->name)) != 0)) {
        node = FindTrieNode(workSpace, auditData->name, strlen(auditData->name), NULL);
        if (node == NULL) {
            node = AddTrieNode(workSpace, auditData->name, strlen(auditData->name));
        }
        PARAM_CHECK(node != NULL, return PARAM_CODE_REACHED_MAX, "Failed to add node %s", auditData->name);
    }
    uint32_t offset = node->labelIndex;
    if (node->labelIndex == 0) {  // can not support update for label
        offset = AddParamSecurityNode(workSpace, auditData);
        PARAM_CHECK(offset > 0, return PARAM_CODE_REACHED_MAX, "Failed to add label");
        SaveIndex(&node->labelIndex, offset);
    } else {
        ParamSecurityNode *label = (ParamSecurityNode *)GetTrieNode(workSpace, node->labelIndex);
        PARAM_CHECK(label != NULL, return -1, "Failed to get trie node");
#ifdef PARAM_SUPPORT_SELINUX
        if (auditData->selinuxIndex != 0) {
            label->selinuxIndex = auditData->selinuxIndex;
        } else
#endif
        {
#ifdef STARTUP_INIT_TEST
            label->mode = auditData->dacData.mode;
            label->uid = auditData->dacData.uid;
            label->gid = auditData->dacData.gid;
            label->type = auditData->dacData.paramType & PARAM_TYPE_MASK;
#endif
            PARAM_LOGV("Repeat to add label for name %s", auditData->name);
        }
    }
    PARAM_LOGV("AddSecurityLabel label %d gid %d uid %d mode %o type:%d name: %s", offset,
        auditData->dacData.gid, auditData->dacData.uid, auditData->dacData.mode,
        auditData->dacData.paramType, auditData->name);
    return 0;
}
