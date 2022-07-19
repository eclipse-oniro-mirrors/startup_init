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

#include "param_manager.h"

#include <ctype.h>
#include <limits.h>

#include "param_trie.h"
#include "securec.h"
#include "param_utils.h"

ParamNode *SystemCheckMatchParamWait(const char *name, const char *value)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return NULL, "Invalid space");

    WorkSpace *worksapce = GetWorkSpace(name);
    PARAM_CHECK(worksapce != NULL, return NULL, "Failed to get workspace %s", name);
    PARAM_LOGV("SystemCheckMatchParamWait name %s", name);
    uint32_t nameLength = strlen(name);
    ParamTrieNode *node = FindTrieNode(worksapce, name, nameLength, NULL);
    if (node == NULL || node->dataIndex == 0) {
        return NULL;
    }
    ParamNode *param = (ParamNode *)GetTrieNode(worksapce, node->dataIndex);
    if (param == NULL) {
        return NULL;
    }
    if ((param->keyLength != nameLength) || (strncmp(param->data, name, nameLength) != 0)) {  // compare name
        return NULL;
    }
    ATOMIC_STORE_EXPLICIT(&param->commitId,
        ATOMIC_LOAD_EXPLICIT(&param->commitId, memory_order_relaxed) | PARAM_FLAGS_WAITED, memory_order_release);
    if ((strncmp(value, "*", 1) == 0) || (strcmp(param->data + nameLength + 1, value) == 0)) { // compare value
        return param;
    }
    char *tmp = strstr(value, "*");
    if (tmp != NULL && (strncmp(param->data + nameLength + 1, value, tmp - value) == 0)) {
        return param;
    }
    return NULL;
}

static int ProcessParamTraversal(const WorkSpace *workSpace, const ParamTrieNode *node, const void *cookie)
{
    ParamTraversalContext *context = (ParamTraversalContext *)cookie;
    ParamTrieNode *current = (ParamTrieNode *)node;
    if (current == NULL) {
        return 0;
    }
    if (current->dataIndex == 0) {
        return 0;
    }
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, current->dataIndex);
    if (entry == NULL) {
        return 0;
    }
    if ((strcmp("#", context->prefix) != 0) && (strncmp(entry->data, context->prefix, strlen(context->prefix)) != 0)) {
        return 0;
    }
    uint32_t index = GetParamHandle(workSpace, current->dataIndex, entry->data);
    context->traversalParamPtr(index, context->context);
    return 0;
}

int SystemTraversalParameter(const char *prefix, TraversalParamPtr traversalParameter, void *cookie)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");

    PARAM_CHECK(traversalParameter != NULL, return -1, "The param is null");
    ParamTraversalContext context = {traversalParameter, cookie, "#"};
    if (!(prefix == NULL || strlen(prefix) == 0)) {
        ParamHandle handle = 0;
        // only check for valid parameter
        int ret = ReadParamWithCheck(prefix, DAC_READ, &handle);
        if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
            PARAM_CHECK(ret == 0, return ret, "Forbid to traversal parameters");
        }
        context.prefix = (char *)prefix;
    }

    WorkSpace *workSpace = GetFirstWorkSpace();
    if (workSpace != NULL && strcmp(workSpace->fileName, WORKSPACE_NAME_DAC) == 0) {
        workSpace = GetNextWorkSpace(workSpace);
    }
    while (workSpace != NULL) {
        PARAM_LOGV("SystemTraversalParameter prefix %s in space %s", context.prefix, workSpace->fileName);
        WorkSpace *next = GetNextWorkSpace(workSpace);
        ParamTrieNode *root = NULL;
        if (prefix != NULL && strlen(prefix) != 0) {
            root = FindTrieNode(workSpace, prefix, strlen(prefix), NULL);
        }
        PARAMSPACE_AREA_RD_LOCK(workSpace);
        TraversalTrieNode(workSpace, root, ProcessParamTraversal, (const void *)&context);
        PARAMSPACE_AREA_RW_UNLOCK(workSpace);
        workSpace = next;
    }
    return 0;
}

static int DumpTrieDataNodeTraversal(const WorkSpace *workSpace, const ParamTrieNode *node, const void *cookie)
{
    int verbose = *(int *)cookie;
    ParamTrieNode *current = (ParamTrieNode *)node;
    if (current == NULL) {
        return 0;
    }
    if (verbose) {
        PARAM_DUMP("\tTrie node info [%u,%u,%u] data: %u label: %u key length:%u \n\t  key: %s \n",
            current->left, current->right, current->child,
            current->dataIndex, current->labelIndex, current->length, current->key);
    }
    if (current->dataIndex != 0) {
        ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, current->dataIndex);
        if (entry != NULL) {
            PARAM_DUMP("\tparameter length info [%u, %u] \n\t  param: %s \n",
                entry->keyLength, entry->valueLength, (entry != NULL) ? entry->data : "null");
        }
    }
    if (current->labelIndex != 0 && verbose) {
        ParamSecurityNode *label = (ParamSecurityNode *)GetTrieNode(workSpace, current->labelIndex);
        if (label != NULL) {
            PARAM_DUMP("\tparameter label dac %u %u %o \n\t  label: %s \n",
                label->uid, label->gid, label->mode, (label->length > 0) ? label->data : "null");
        }
    }
    return 0;
}

static void HashNodeTraverseForDump(WorkSpace *workSpace, int verbose)
{
    PARAM_DUMP("    map file: %s \n", workSpace->fileName);
    if (workSpace->area != NULL) {
        PARAM_DUMP("    total size: %u \n", workSpace->area->dataSize);
        PARAM_DUMP("    first offset: %u \n", workSpace->area->firstNode);
        PARAM_DUMP("    current offset: %u \n", workSpace->area->currOffset);
        PARAM_DUMP("    total node: %u \n", workSpace->area->trieNodeCount);
        PARAM_DUMP("    total param node: %u \n", workSpace->area->paramNodeCount);
        PARAM_DUMP("    total security node: %u\n", workSpace->area->securityNodeCount);
    }
    PARAM_DUMP("    node info: \n");
    PARAMSPACE_AREA_RD_LOCK(workSpace);
    TraversalTrieNode(workSpace, NULL, DumpTrieDataNodeTraversal, (const void *)&verbose);
    PARAMSPACE_AREA_RW_UNLOCK(workSpace);
}

void SystemDumpParameters(int verbose)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return, "Invalid space");
    // check default dac
    ParamHandle handle = 0;
    int ret = ReadParamWithCheck("#", DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return, "Forbid to dump parameters");
    }
    PARAM_DUMP("Dump all parameters begin ...\n");
    if (verbose) {
        PARAM_DUMP("Local sercurity information\n");
        PARAM_DUMP("\t pid: %d uid: %u gid: %u \n",
            paramSpace->securityLabel.cred.pid,
            paramSpace->securityLabel.cred.uid,
            paramSpace->securityLabel.cred.gid);
    }
    WorkSpace *workSpace = GetFirstWorkSpace();
    while (workSpace != NULL) {
        WorkSpace *next = GetNextWorkSpace(workSpace);
        HashNodeTraverseForDump(workSpace, verbose);
        workSpace = next;
    }
    PARAM_DUMP("Dump all parameters finish\n");
}

INIT_INNER_API int SysCheckParamExist(const char *name)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_CHECK(name != NULL, return -1, "The name or handle is null");

    WorkSpace *workSpace = GetFirstWorkSpace();
    while (workSpace != NULL) {
        PARAM_LOGV("SysCheckParamExist name %s in space %s", name, workSpace->fileName);
        WorkSpace *next = GetNextWorkSpace(workSpace);
        ParamTrieNode *node = FindTrieNode(workSpace, name, strlen(name), NULL);
        if (node != NULL && node->dataIndex != 0) {
            return 0;
        } else if (node != NULL) {
            return PARAM_CODE_NODE_EXIST;
        }
        workSpace = next;
    }
    return PARAM_CODE_NOT_FOUND;
}

INIT_INNER_API int GetParamSecurityAuditData(const char *name, int type, ParamAuditData *auditData)
{
    UNUSED(type);
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    uint32_t labelIndex = 0;
    // get from dac
    WorkSpace *space = GetWorkSpace(WORKSPACE_NAME_DAC);
    PARAM_CHECK(space != NULL, return -1, "Invalid workSpace");
    FindTrieNode(space, name, strlen(name), &labelIndex);
    ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(space, labelIndex);
    PARAM_CHECK(node != NULL, return DAC_RESULT_FORBIDED, "Can not get security label %d", labelIndex);

    auditData->name = name;
    auditData->dacData.uid = node->uid;
    auditData->dacData.gid = node->gid;
    auditData->dacData.mode = node->mode;
#ifdef PARAM_SUPPORT_SELINUX
    const char *tmpName = GetSelinuxContent(name);
    if (tmpName != NULL) {
        int ret = strcpy_s(auditData->label, sizeof(auditData->label), tmpName);
        PARAM_CHECK(ret == 0, return 0, "Failed to copy label for %s", name);
    }
#endif
    return 0;
}

static char *BuildKey(const char *format, ...)
{
    const size_t buffSize = 1024;  // 1024 for format key
    char *buffer = malloc(buffSize);
    PARAM_CHECK(buffer != NULL, return NULL, "Failed to malloc for format");
    va_list vargs;
    va_start(vargs, format);
    int len = vsnprintf_s(buffer, buffSize, buffSize - 1, format, vargs);
    va_end(vargs);
    if (len > 0 && (size_t)len < buffSize) {
        buffer[len] = '\0';
        for (int i = 0; i < len; i++) {
            if (buffer[i] == '|') {
                buffer[i] = '\0';
            }
        }
        return buffer;
    }
    return NULL;
}

PARAM_STATIC char *GetServiceCtrlName(const char *name, const char *value)
{
    static char *ctrlParam[] = {
        "ohos.ctl.start",
        "ohos.ctl.stop"
    };
    static char *installParam[] = {
        "ohos.servicectrl."
    };
    static char *powerCtrlArg[][2] = {
        {"reboot,shutdown", "reboot.shutdown"},
        {"reboot,updater", "reboot.updater"},
        {"reboot,flashd", "reboot.flashd"},
        {"reboot", "reboot"},
    };
    char *key = NULL;
    if (strcmp("ohos.startup.powerctrl", name) == 0) {
        for (size_t i = 0; i < ARRAY_LENGTH(powerCtrlArg); i++) {
            if (strncmp(value, powerCtrlArg[i][0], strlen(powerCtrlArg[i][0])) == 0) {
                return BuildKey("%s%s", OHOS_SERVICE_CTRL_PREFIX, powerCtrlArg[i][1]);
            }
        }
        return key;
    }
    for (size_t i = 0; i < ARRAY_LENGTH(ctrlParam); i++) {
        if (strcmp(name, ctrlParam[i]) == 0) {
            return BuildKey("%s%s", OHOS_SERVICE_CTRL_PREFIX, value);
        }
    }

    for (size_t i = 0; i < ARRAY_LENGTH(installParam); i++) {
        if (strncmp(name, installParam[i], strlen(installParam[i])) == 0) {
            return BuildKey("%s.%s", name, value);
        }
    }
    return key;
}

INIT_INNER_API int CheckParameterSet(const char *name,
    const char *value, const ParamSecurityLabel *srcLabel, int *ctrlService)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_LOGV("CheckParameterSet name %s value: %s", name, value);
    PARAM_CHECK(srcLabel != NULL && ctrlService != NULL, return -1, "Invalid param ");
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", name);
    ret = CheckParamValue(NULL, name, value);
    PARAM_CHECK(ret == 0, return ret, "Illegal param value %s", value);
    *ctrlService = 0;

#ifndef __LITEOS_M__
    if (getpid() != 1) { // none init
#ifdef PARAM_SUPPORT_SELINUX
        *ctrlService |= PARAM_NEED_CHECK_IN_SERVICE;
        return 0;
#else
        if ((srcLabel->flags[0] & LABEL_CHECK_IN_ALL_PROCESS) != LABEL_CHECK_IN_ALL_PROCESS) {
            *ctrlService |= PARAM_NEED_CHECK_IN_SERVICE;
            return 0;
        }
#endif
    }
#endif
    char *key = GetServiceCtrlName(name, value);
    ret = CheckParamPermission(srcLabel, (key == NULL) ? name : key, DAC_WRITE);
    if (key != NULL) {  // ctrl param
        free(key);
        *ctrlService |= PARAM_CTRL_SERVICE;
    }
    return ret;
}
