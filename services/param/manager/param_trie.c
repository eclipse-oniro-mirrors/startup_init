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

#include "param_trie.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "param_utils.h"
#include "sys_param.h"

static int InitWorkSpace_(WorkSpace *workSpace, int mode, int prot, uint32_t spaceSize, int readOnly)
{
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    PARAM_CHECK(spaceSize <= PARAM_WORKSPACE_MAX, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    PARAM_CHECK(workSpace->allocTrieNode != NULL,
        return PARAM_CODE_INVALID_PARAM, "Invalid allocTrieNode %s", workSpace->fileName);
    PARAM_CHECK(workSpace->compareTrieNode != NULL,
        return PARAM_CODE_INVALID_PARAM, "Invalid compareTrieNode %s", workSpace->fileName);
    PARAM_LOGD("InitWorkSpace %s ", workSpace->fileName);
    CheckAndCreateDir(workSpace->fileName);

    int fd = open(workSpace->fileName, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    PARAM_CHECK(fd >= 0, return PARAM_CODE_INVALID_NAME,
        "Open file %s mode %x fail error %d", workSpace->fileName, mode, errno);

    if (!readOnly) {
        ftruncate(fd, spaceSize);
    }
    void *areaAddr = (void *)mmap(NULL, spaceSize, prot, MAP_SHARED, fd, 0);
    PARAM_CHECK(areaAddr != MAP_FAILED && areaAddr != NULL, close(fd);
        return PARAM_CODE_ERROR_MAP_FILE, "Failed to map memory error %d", errno);
    close(fd);

    if (!readOnly) {
        workSpace->area = (ParamTrieHeader *)areaAddr;
        workSpace->area->trieNodeCount = 0;
        workSpace->area->paramNodeCount = 0;
        workSpace->area->securityNodeCount = 0;
        workSpace->area->dataSize = spaceSize - sizeof(ParamTrieHeader);
        workSpace->area->currOffset = 0;
        uint32_t offset = workSpace->allocTrieNode(workSpace, "#", 1);
        workSpace->area->firstNode = offset;
    } else {
        workSpace->area = (ParamTrieHeader *)areaAddr;
    }
    PARAM_LOGD("InitWorkSpace success, readOnly %d currOffset %u firstNode %u dataSize %u",
        readOnly, workSpace->area->currOffset, workSpace->area->firstNode, workSpace->area->dataSize);
    return 0;
}

static uint32_t AllocateParamTrieNode(WorkSpace *workSpace, const char *key, uint32_t keyLen)
{
    uint32_t len = keyLen + sizeof(ParamTrieNode) + 1;
    len = PARAM_ALIGN(len);
    PARAM_CHECK((workSpace->area->currOffset + len) < workSpace->area->dataSize, return 0,
        "Failed to allocate currOffset %d, dataSize %d", workSpace->area->currOffset, workSpace->area->dataSize);
    ParamTrieNode *node = (ParamTrieNode*)(workSpace->area->data + workSpace->area->currOffset);
    node->length = keyLen;
    int ret = memcpy_s(node->key, keyLen, key, keyLen);
    PARAM_CHECK(ret == EOK, return 0, "Failed to copy key");
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

int InitWorkSpace(const char *fileName, WorkSpace *workSpace, int onlyRead)
{
    PARAM_CHECK(fileName != NULL, return PARAM_CODE_INVALID_NAME, "Invalid fileName");
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_NAME, "Invalid workSpace");
    if (workSpace->area != NULL) {
        return 0;
    }
    workSpace->compareTrieNode = CompareParamTrieNode;
    workSpace->allocTrieNode = AllocateParamTrieNode;
    int ret = strcpy_s(workSpace->fileName, sizeof(workSpace->fileName), fileName);
    PARAM_CHECK(ret == 0, return ret, "Failed to copy file name %s", fileName);
    int openMode;
    int prot = PROT_READ;
    if (onlyRead) {
        openMode = O_RDONLY;
    } else {
        openMode = O_CREAT | O_RDWR | O_TRUNC;
        prot = PROT_READ | PROT_WRITE;
    }
    ret = InitWorkSpace_(workSpace, openMode, prot, PARAM_WORKSPACE_MAX, onlyRead);
    PARAM_CHECK(ret == 0, return ret, "Failed to init workspace  %s", workSpace->fileName);
    return ret;
}

void CloseWorkSpace(WorkSpace *workSpace)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return, "The workspace is null");
    munmap((char *)workSpace->area, workSpace->area->dataSize);
    workSpace->area = NULL;
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
            PARAM_CHECK(offset != 0, return NULL, "Failed to allocate key %s", key);
            SaveIndex(&current->left, offset);
            return GetTrieNode(workSpace, current->left);
        }
    } else {
        subTrie = GetTrieNode(workSpace, current->right);
        if (subTrie == NULL) {
            uint32_t offset = workSpace->allocTrieNode(workSpace, key, keyLen);
            PARAM_CHECK(offset != 0, return NULL, "Failed to allocate key %s", key);
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
        if (current->child != 0) { // 如果child存在，则检查是否匹配
            ParamTrieNode *next = GetTrieNode(workSpace, current->child);
            current = AddToSubTrie(workSpace, next, remainingKey, subKeyLen);
        } else {
            uint32_t dataOffset = workSpace->allocTrieNode(workSpace, remainingKey, subKeyLen);
            PARAM_CHECK(dataOffset != 0, return NULL, "Failed to allocate key %s", key);
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
        if (matchLabel != NULL && current->labelIndex != 0) { // 有效前缀
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

ParamTrieNode *FindTrieNode(const WorkSpace *workSpace, const char *key, uint32_t keyLen, uint32_t *matchLabel)
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
    const ParamTrieNode *current, TraversalTrieNodePtr walkFunc, void *cookie)
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

int TraversalTrieNode(const WorkSpace *workSpace,
    const ParamTrieNode *root, TraversalTrieNodePtr walkFunc, void *cookie)
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

uint32_t AddParamSecruityNode(WorkSpace *workSpace, const ParamAuditData *auditData)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return 0, "Invalid param");
    PARAM_CHECK(auditData != NULL && auditData->name != NULL, return 0, "Invalid auditData");
    const uint32_t labelLen = (auditData->label == NULL) ? 0 : strlen(auditData->label);
    uint32_t realLen = sizeof(ParamSecruityNode) + PARAM_ALIGN(labelLen + 1);
    PARAM_CHECK((workSpace->area->currOffset + realLen) < workSpace->area->dataSize, return 0,
        "Failed to allocate currOffset %u, dataSize %u datalen %u",
        workSpace->area->currOffset, workSpace->area->dataSize, realLen);

    ParamSecruityNode *node = (ParamSecruityNode *)(workSpace->area->data + workSpace->area->currOffset);
    node->uid = auditData->dacData.uid;
    node->gid = auditData->dacData.gid;
    node->mode = auditData->dacData.mode;
    node->length = 0;
    if (labelLen != 0) {
        int ret = memcpy_s(node->data, labelLen, auditData->label, labelLen);
        PARAM_CHECK(ret == EOK, return 0, "Failed to copy key");
        node->data[labelLen] = '\0';
        node->length = labelLen;
    }
    uint32_t offset = workSpace->area->currOffset;
    workSpace->area->currOffset += realLen;
    workSpace->area->securityNodeCount++;
    return offset;
}

uint32_t AddParamNode(WorkSpace *workSpace, const char *key, uint32_t keyLen, const char *value, uint32_t valueLen)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return 0, "Invalid param");
    PARAM_CHECK(key != NULL && value != NULL, return 0, "Invalid param");

    uint32_t realLen = sizeof(ParamNode) + 1 + 1;
    if (valueLen > PARAM_VALUE_LEN_MAX) {
        realLen += keyLen + valueLen;
    } else {
        realLen += keyLen + PARAM_VALUE_LEN_MAX;
    }
    realLen = PARAM_ALIGN(realLen);
    PARAM_CHECK((workSpace->area->currOffset + realLen) < workSpace->area->dataSize, return 0,
        "Failed to allocate currOffset %u, dataSize %u datalen %u",
        workSpace->area->currOffset, workSpace->area->dataSize, realLen);

    ParamNode *node = (ParamNode *)(workSpace->area->data + workSpace->area->currOffset);
    atomic_init(&node->commitId, 0);

    node->keyLength = keyLen;
    node->valueLength = valueLen;
    int ret = sprintf_s(node->data, realLen - 1, "%s=%s", key, value);
    PARAM_CHECK(ret > EOK, return 0, "Failed to sprint key and value");
    uint32_t offset = workSpace->area->currOffset;
    workSpace->area->currOffset += realLen;
    workSpace->area->paramNodeCount++;
    return offset;
}

ParamTrieNode *GetTrieNode(const WorkSpace *workSpace, uint32_t offset)
{
    PARAM_CHECK(workSpace != NULL && workSpace->area != NULL, return NULL, "Invalid param");
    if (offset == 0 || offset > workSpace->area->dataSize) {
        return NULL;
    }
    return (ParamTrieNode *)(workSpace->area->data + offset);
}

void SaveIndex(uint32_t *index, uint32_t offset)
{
    PARAM_CHECK(index != NULL, return, "Invalid index");
    *index = offset;
}
