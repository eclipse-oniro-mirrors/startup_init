/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

static int GetRealFileName(WorkSpace *workSpace, char *buffer, uint32_t size)
{
    int ret = ParamSprintf(buffer, size, "%s/%s", PARAM_STORAGE_PATH, workSpace->fileName);
    PARAM_CHECK(ret > 0, return -1, "Failed to copy file name %s", workSpace->fileName);
    buffer[ret] = '\0';
    return 0;
}

static int InitWorkSpace_(WorkSpace *workSpace, uint32_t spaceSize, int readOnly)
{
    static uint32_t startIndex = 0;
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    PARAM_CHECK(sizeof(ParamTrieHeader) < spaceSize,
        return PARAM_CODE_INVALID_PARAM, "Invalid spaceSize %u", spaceSize);
    PARAM_CHECK(workSpace->allocTrieNode != NULL,
        return PARAM_CODE_INVALID_PARAM, "Invalid allocTrieNode %s", workSpace->fileName);
    PARAM_CHECK(workSpace->compareTrieNode != NULL,
        return PARAM_CODE_INVALID_PARAM, "Invalid compareTrieNode %s", workSpace->fileName);

    char buffer[FILENAME_LEN_MAX] = {0};
    int ret = GetRealFileName(workSpace, buffer, sizeof(buffer));
    PARAM_CHECK(ret == 0, return -1, "Failed to get file name %s", workSpace->fileName);
    void *areaAddr = GetSharedMem(buffer, &workSpace->memHandle, spaceSize, readOnly);
    PARAM_ONLY_CHECK(areaAddr != NULL, return PARAM_CODE_ERROR_MAP_FILE);
    if (!readOnly) {
        workSpace->area = (ParamTrieHeader *)areaAddr;
        ATOMIC_INIT(&workSpace->area->commitId, 0);
        ATOMIC_INIT(&workSpace->area->commitPersistId, 0);
        workSpace->area->trieNodeCount = 0;
        workSpace->area->paramNodeCount = 0;
        workSpace->area->securityNodeCount = 0;
        workSpace->area->startIndex = startIndex;
        startIndex += spaceSize;
        workSpace->area->dataSize = spaceSize - sizeof(ParamTrieHeader);
        workSpace->area->currOffset = 0;
        uint32_t offset = workSpace->allocTrieNode(workSpace, "#", 1);
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
        "Failed to allocate currOffset %d, dataSize %d", workSpace->area->currOffset, workSpace->area->dataSize);
    ParamTrieNode *node = (ParamTrieNode *)(workSpace->area->data + workSpace->area->currOffset);
    node->length = keyLen;
    int ret = ParamMemcpy(node->key, keyLen, key, keyLen);
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

static int CompareParamTrieNode(const ParamTrieNode *node, const char *key, uint32_t keyLen)
{
    if (node->length > keyLen) {
        return -1;
    } else if (node->length < keyLen) {
        return 1;
    }
    return strncmp(node->key, key, keyLen);
}

INIT_LOCAL_API int InitWorkSpace(WorkSpace *workSpace, int onlyRead, uint32_t spaceSize)
{
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_NAME, "Invalid workSpace");
    if (PARAM_TEST_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
    workSpace->compareTrieNode = CompareParamTrieNode;
    workSpace->allocTrieNode = AllocateParamTrieNode;
    workSpace->area = NULL;
    int ret = InitWorkSpace_(workSpace, spaceSize, onlyRead);
    PARAM_ONLY_CHECK(ret == 0, return ret);
    PARAMSPACE_AREA_INIT_LOCK(workSpace);
    PARAM_SET_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT);
    PARAM_LOGV("InitWorkSpace %s for %s", workSpace->fileName, (onlyRead == 0) ? "init" : "other");
    return ret;
}

INIT_LOCAL_API void CloseWorkSpace(WorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL, return, "The workspace is null");
    PARAM_LOGV("CloseWorkSpace %s", workSpace->fileName);
    if (!PARAM_TEST_FLAG(workSpace->flags, WORKSPACE_FLAGS_INIT)) {
        free(workSpace);
        return;
    }
    OH_ListRemove(&workSpace->node);
    PARAM_CHECK(workSpace->area != NULL, return, "The workspace area is null");
#ifdef WORKSPACE_AREA_NEED_MUTEX
    ParamRWMutexDelete(&workSpace->rwlock);
#endif
    FreeSharedMem(&workSpace->memHandle, workSpace->area, workSpace->area->dataSize);
    workSpace->area = NULL;
    free(workSpace);
}

static ParamTrieNode *GetTrieRoot(const WorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return NULL, "The workspace is null");
    return (ParamTrieNode *)(workSpace->area->data + workSpace->area->firstNode);
}

static void GetNextKey(const char **remainingKey, char **subKey, uint32_t *subKeyLen)
{
    *subKey = strchr(*remainingKey, '.');
    if (*subKey != NULL) {
        *subKeyLen = *subKey - *remainingKey;
    } else {
        *subKeyLen = strlen(*remainingKey);
    }
}

static ParamTrieNode *AddToSubTrie(WorkSpace *workSpace, ParamTrieNode *current, const char *key, uint32_t keyLen)
{
    if (current == NULL || workSpace == NULL || key == NULL) {
        return NULL;
    }
    ParamTrieNode *subTrie = NULL;
    int ret = workSpace->compareTrieNode(current, key, keyLen);
    if (ret == 0) {
        return current;
    }
    if (ret < 0) {
        subTrie = GetTrieNode(workSpace, current->left);
        if (subTrie == NULL) {
            uint32_t offset = workSpace->allocTrieNode(workSpace, key, keyLen);
            PARAM_CHECK(offset != 0, return NULL,
                "Failed to allocate key '%s' in space '%s'", key, workSpace->fileName);
            SaveIndex(&current->left, offset);
            return GetTrieNode(workSpace, current->left);
        }
    } else {
        subTrie = GetTrieNode(workSpace, current->right);
        if (subTrie == NULL) {
            uint32_t offset = workSpace->allocTrieNode(workSpace, key, keyLen);
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
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return 0, "Invalid workSpace %s", key);
    PARAM_CHECK(workSpace->allocTrieNode != NULL, return NULL, "Invalid param %s", key);
    PARAM_CHECK(workSpace->compareTrieNode != NULL, return NULL, "Invalid param %s", key);
    const char *remainingKey = key;
    ParamTrieNode *current = GetTrieRoot(workSpace);
    PARAM_CHECK(current != NULL, return NULL, "Invalid current param %s", key);
    while (1) {
        uint32_t subKeyLen = 0;
        char *subKey = NULL;
        GetNextKey(&remainingKey, &subKey, &subKeyLen);
        if (!subKeyLen) {
            return NULL;
        }
        if (current->child != 0) {  // 如果child存在，则检查是否匹配
            ParamTrieNode *next = GetTrieNode(workSpace, current->child);
            current = AddToSubTrie(workSpace, next, remainingKey, subKeyLen);
        } else {
            uint32_t dataOffset = workSpace->allocTrieNode(workSpace, remainingKey, subKeyLen);
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

static ParamTrieNode *FindSubTrie(const WorkSpace *workSpace,
    ParamTrieNode *current, const char *key, uint32_t keyLen, uint32_t *matchLabel)
{
    if (current == NULL) {
        return NULL;
    }
    ParamTrieNode *subTrie = NULL;
    int ret = workSpace->compareTrieNode(current, key, keyLen);
    if (ret == 0) {
        if (matchLabel != NULL && current->labelIndex != 0) {
            *matchLabel = current->labelIndex;
        }
        return current;
    }
    if (ret < 0) {
        subTrie = (ParamTrieNode *)GetTrieNode(workSpace, current->left);
        if (subTrie == NULL) {
            return NULL;
        }
    } else {
        subTrie = (ParamTrieNode *)GetTrieNode(workSpace, current->right);
        if (subTrie == NULL) {
            return NULL;
        }
    }
    return FindSubTrie(workSpace, subTrie, key, keyLen, matchLabel);
}

static ParamTrieNode *FindTrieNode_(const WorkSpace *workSpace, const char *key, uint32_t keyLen, uint32_t *matchLabel)
{
    PARAM_CHECK(key != NULL && keyLen > 0, return NULL, "Invalid key ");
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return 0, "Invalid workSpace %s", key);
    PARAM_CHECK(workSpace->allocTrieNode != NULL, return NULL, "Invalid alloc function %s", key);
    PARAM_CHECK(workSpace->compareTrieNode != NULL, return NULL, "Invalid compare function %s", key);
    const char *remainingKey = key;
    ParamTrieNode *current = GetTrieRoot(workSpace);
    PARAM_CHECK(current != NULL, return NULL, "Invalid current param %s", key);
    if (matchLabel != NULL) {
        *matchLabel = current->labelIndex;
    }
    int ret = workSpace->compareTrieNode(current, key, keyLen);
    if (ret == 0) {
        return current;
    }
    while (1) {
        uint32_t subKeyLen = 0;
        char *subKey = NULL;
        GetNextKey(&remainingKey, &subKey, &subKeyLen);
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
        } else if (matchLabel != NULL && current->labelIndex != 0) {
            *matchLabel = current->labelIndex;
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
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return 0, "Invalid workSpace");
    ParamTrieNode *current = (ParamTrieNode *)root;
    if (root == NULL) {
        current = GetTrieRoot(workSpace);
    }
    if (current == NULL) {
        return 0;
    }
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
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return 0, "Invalid param");
    PARAM_CHECK(auditData != NULL && auditData->name != NULL, return 0, "Invalid auditData");
    uint32_t realLen = sizeof(ParamSecurityNode);
    PARAM_CHECK((workSpace->area->currOffset + realLen) < workSpace->area->dataSize, return 0,
        "Failed to allocate currOffset %u, dataSize %u datalen %u",
        workSpace->area->currOffset, workSpace->area->dataSize, realLen);
    ParamSecurityNode *node = (ParamSecurityNode *)(workSpace->area->data + workSpace->area->currOffset);
    node->uid = auditData->dacData.uid;
    node->gid = auditData->dacData.gid;
    node->mode = auditData->dacData.mode;
    node->type = auditData->dacData.paramType & PARAM_TYPE_MASK;
    uint32_t offset = workSpace->area->currOffset;
    workSpace->area->currOffset += realLen;
    workSpace->area->securityNodeCount++;
    return offset;
}

INIT_LOCAL_API uint32_t AddParamNode(WorkSpace *workSpace, uint8_t type,
    const char *key, uint32_t keyLen, const char *value, uint32_t valueLen)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return 0, "Invalid param");
    PARAM_CHECK(key != NULL && value != NULL, return 0, "Invalid param");

    uint32_t realLen = sizeof(ParamNode) + 1 + 1;
    // for const parameter, alloc memory on demand
    if ((valueLen > PARAM_VALUE_LEN_MAX) || IS_READY_ONLY(key)) {
        realLen += keyLen + valueLen;
    } else {
        realLen += keyLen + GetParamMaxLen(type);
    }
    realLen = PARAM_ALIGN(realLen);
    PARAM_CHECK((workSpace->area->currOffset + realLen) < workSpace->area->dataSize, return 0,
        "Failed to allocate currOffset %u, dataSize %u datalen %u",
        workSpace->area->currOffset, workSpace->area->dataSize, realLen);

    ParamNode *node = (ParamNode *)(workSpace->area->data + workSpace->area->currOffset);
    ATOMIC_INIT(&node->commitId, 0);

    node->type = type;
    node->keyLength = keyLen;
    node->valueLength = valueLen;
    int ret = ParamSprintf(node->data, realLen, "%s=%s", key, value);
    PARAM_CHECK(ret > 0, return 0, "Failed to sprint key and value");
    uint32_t offset = workSpace->area->currOffset;
    workSpace->area->currOffset += realLen;
    workSpace->area->paramNodeCount++;
    return offset;
}

INIT_LOCAL_API ParamTrieNode *GetTrieNode(const WorkSpace *workSpace, uint32_t offset)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return NULL, "Invalid param");
    if (offset == 0 || offset > workSpace->area->dataSize) {
        return NULL;
    }
    return (ParamTrieNode *)(workSpace->area->data + offset);
}

INIT_LOCAL_API void SaveIndex(uint32_t *index, uint32_t offset)
{
    PARAM_CHECK(index != NULL, return, "Invalid index");
    *index = offset;
}

INIT_LOCAL_API ParamTrieNode *FindTrieNode(WorkSpace *workSpace,
    const char *key, uint32_t keyLen, uint32_t *matchLabel)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return NULL, "Invalid workSpace");
    ParamTrieNode *node = NULL;
    PARAMSPACE_AREA_RD_LOCK(workSpace);
    node = FindTrieNode_(workSpace, key, keyLen, matchLabel);
    PARAMSPACE_AREA_RW_UNLOCK(workSpace);
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

INIT_LOCAL_API ParamNode *GetParamNode(const char *spaceName, const char *name)
{
    uint32_t labelIndex = 0;
    WorkSpace *space = GetWorkSpace(spaceName);
    PARAM_CHECK(space != NULL, return NULL, "Failed to get dac space %s", name);
    ParamTrieNode *entry = FindTrieNode(space, name, strlen(name), &labelIndex);
    if (entry == NULL || entry->dataIndex == 0) {
        return NULL;
    }
    return (ParamNode *)GetTrieNode(space, entry->dataIndex);
}