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

#include "param_manager.h"

#include <ctype.h>
#include <limits.h>

#include "init_cmds.h"
#include "init_hook.h"
#include "param_trie.h"
#include "param_utils.h"
#include "securec.h"
static DUMP_PRINTF g_printf = printf;

static int ReadParamName(ParamHandle handle, char *name, uint32_t length);

ParamNode *SystemCheckMatchParamWait(const char *name, const char *value)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return NULL, "Invalid space");

    WorkSpace *workspace = GetWorkSpace(GetWorkSpaceIndex(name));
    PARAM_CHECK(workspace != NULL, return NULL, "Failed to get workspace %s", name);
    PARAM_LOGV("SystemCheckMatchParamWait name %s", name);
    uint32_t nameLength = strlen(name);
    ParamTrieNode *node = FindTrieNode(workspace, name, nameLength, NULL);
    if (node == NULL || node->dataIndex == 0) {
        return NULL;
    }
    ParamNode *param = (ParamNode *)GetTrieNode(workspace, node->dataIndex);
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
    uint32_t index = PARAM_HANDLE(workSpace, current->dataIndex);
    context->traversalParamPtr(index, context->context);
    return 0;
}

int SystemTraversalParameter(const char *prefix, TraversalParamPtr traversalParameter, void *cookie)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_CHECK(traversalParameter != NULL, return -1, "The param is null");

#ifdef PARAM_SUPPORT_SELINUX // load security label
    ParamSecurityOps *ops = GetParamSecurityOps(PARAM_SECURITY_SELINUX);
    if (ops != NULL && ops->securityGetLabel != NULL) {
        ops->securityGetLabel("open");
    }
#endif
    ParamTraversalContext context = {traversalParameter, cookie, "#"};
    if (!(prefix == NULL || strlen(prefix) == 0)) {
        int ret = CheckParamPermission(GetParamSecurityLabel(), prefix, DAC_READ);
        PARAM_CHECK(ret == 0, return ret, "Forbid to traversal parameters %s", prefix);
        context.prefix = (char *)prefix;
    }

    WorkSpace *workSpace = GetNextWorkSpace(NULL);
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
                entry->keyLength, entry->valueLength, entry->data);
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

void SystemDumpParameters(int verbose, int index, int (*dump)(const char *fmt, ...))
{
    if (dump != NULL) {
        g_printf = dump;
    } else {
        g_printf = printf;
    }
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return, "Invalid space");
    // check default dac
    int ret = CheckParamPermission(GetParamSecurityLabel(), "#", DAC_READ);
    PARAM_CHECK(ret == 0, return, "Forbid to dump parameters ");
#ifdef PARAM_SUPPORT_SELINUX // load security label
    ParamSecurityOps *ops = GetParamSecurityOps(PARAM_SECURITY_SELINUX);
    if (ops != NULL && ops->securityGetLabel != NULL) {
        ops->securityGetLabel("open");
    }
#endif
    PARAM_DUMP("Dump all parameters begin ...\n");
    if (verbose) {
        PARAM_DUMP("Local security information\n");
        PARAM_DUMP("pid: %d uid: %u gid: %u \n",
            paramSpace->securityLabel.cred.pid,
            paramSpace->securityLabel.cred.uid,
            paramSpace->securityLabel.cred.gid);
    }
    if (index > 0) {
        WorkSpace *workSpace = GetWorkSpace(index);
        if (workSpace != NULL) {
            HashNodeTraverseForDump(workSpace, verbose);
        }
        return;
    }
    WorkSpace *workSpace = GetNextWorkSpace(NULL);
    while (workSpace != NULL) {
        WorkSpace *next = GetNextWorkSpace(workSpace);
        HashNodeTraverseForDump(workSpace, verbose);
        workSpace = next;
    }
    PARAM_DUMP("Dump all parameters finish\n");
}

INIT_LOCAL_API int SysCheckParamExist(const char *name)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_CHECK(name != NULL, return -1, "The name or handle is null");

    WorkSpace *workSpace = GetNextWorkSpace(NULL);
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
    WorkSpace *space = GetWorkSpace(WORKSPACE_INDEX_DAC);
    PARAM_CHECK(space != NULL, return -1, "Invalid workSpace");
    FindTrieNode(space, name, strlen(name), &labelIndex);
    ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(space, labelIndex);
    PARAM_CHECK(node != NULL, return DAC_RESULT_FORBIDED, "Can not get security label %d", labelIndex);

    auditData->name = name;
    auditData->dacData.uid = node->uid;
    auditData->dacData.gid = node->gid;
    auditData->dacData.mode = node->mode;
#ifdef PARAM_SUPPORT_SELINUX
    const char *tmpName = (paramSpace->selinuxSpace.getParamLabel != NULL) ?
        paramSpace->selinuxSpace.getParamLabel(name) : NULL;
    if (tmpName != NULL) {
        int ret = strcpy_s(auditData->label, sizeof(auditData->label), tmpName);
        PARAM_CHECK(ret == 0, return 0, "Failed to copy label for %s", name);
    }
#endif
    return 0;
}

static int CreateCtrlInfo(ServiceCtrlInfo **ctrlInfo, const char *cmd, uint32_t offset,
    uint8_t ctrlParam, const char *format, ...)
{
    *ctrlInfo = calloc(1, sizeof(ServiceCtrlInfo));
    PARAM_CHECK(ctrlInfo != NULL, return -1, "Failed to alloc memory %s", cmd);
    va_list vargs;
    va_start(vargs, format);
    int len = vsnprintf_s((*ctrlInfo)->realKey,
        sizeof((*ctrlInfo)->realKey), sizeof((*ctrlInfo)->realKey) - 1, format, vargs);
    va_end(vargs);
    int ret = strcpy_s((*ctrlInfo)->cmdName, sizeof((*ctrlInfo)->cmdName), cmd);
    (*ctrlInfo)->valueOffset = offset;
    if (ret != 0 || len <= 0) {
        free(*ctrlInfo);
        return -1;
    }
    (*ctrlInfo)->ctrlParam = ctrlParam;
    return 0;
}

static int GetServiceCtrlInfoForPowerCtrl(const char *name, const char *value, ServiceCtrlInfo **ctrlInfo)
{
    size_t size = 0;
    const ParamCmdInfo *powerCtrlArg = GetStartupPowerCtl(&size);
    PARAM_CHECK(powerCtrlArg != NULL, return -1, "Invalid ctrlInfo for %s", name);
    uint32_t valueOffset = strlen(OHOS_SERVICE_CTRL_PREFIX) + strlen("reboot") + 1;
    if (strcmp(value, "reboot") == 0) {
        return CreateCtrlInfo(ctrlInfo, "reboot", valueOffset, 1,
            "%s%s.%s", OHOS_SERVICE_CTRL_PREFIX, "reboot", value);
    }
    for (size_t i = 0; i < size; i++) {
        PARAM_LOGV("Get power ctrl %s name %s value %s", powerCtrlArg[i].name, name, value);
        if (strncmp(value, powerCtrlArg[i].name, strlen(powerCtrlArg[i].name)) == 0) {
            valueOffset = strlen(OHOS_SERVICE_CTRL_PREFIX) + strlen(powerCtrlArg[i].replace) + 1;
            return CreateCtrlInfo(ctrlInfo, powerCtrlArg[i].cmd, valueOffset, 1,
                "%s%s.%s", OHOS_SERVICE_CTRL_PREFIX, powerCtrlArg[i].replace, value);
        }
    }
    // not found reboot, so reboot by normal
    valueOffset = strlen(OHOS_SERVICE_CTRL_PREFIX) + strlen("reboot") + 1;
    return CreateCtrlInfo(ctrlInfo, "reboot.other", valueOffset, 1, "%s%s.%s",
        OHOS_SERVICE_CTRL_PREFIX, "reboot", value);
}

INIT_LOCAL_API int GetServiceCtrlInfo(const char *name, const char *value, ServiceCtrlInfo **ctrlInfo)
{
    PARAM_CHECK(ctrlInfo != NULL, return -1, "Invalid ctrlInfo %s", name);
    *ctrlInfo = NULL;
    size_t size = 0;
    if (strcmp("ohos.startup.powerctrl", name) == 0) {
        return GetServiceCtrlInfoForPowerCtrl(name, value, ctrlInfo);
    }
    if (strncmp("ohos.ctl.", name, strlen("ohos.ctl.")) == 0) {
        const ParamCmdInfo *ctrlParam = GetServiceStartCtrl(&size);
        PARAM_CHECK(ctrlParam != NULL, return -1, "Invalid ctrlInfo for %s", name);
        for (size_t i = 0; i < size; i++) {
            if (strcmp(name, ctrlParam[i].name) == 0) {
                uint32_t valueOffset = strlen(OHOS_SERVICE_CTRL_PREFIX) + strlen(ctrlParam[i].replace) + 1;
                return CreateCtrlInfo(ctrlInfo, ctrlParam[i].cmd, valueOffset, 1,
                    "%s%s.%s", OHOS_SERVICE_CTRL_PREFIX, ctrlParam[i].replace, value);
            }
        }
    }
    if (strncmp("ohos.servicectrl.", name, strlen("ohos.servicectrl.")) == 0) {
        const ParamCmdInfo *installParam = GetServiceCtl(&size);
        PARAM_CHECK(installParam != NULL, return -1, "Invalid ctrlInfo for %s", name);
        for (size_t i = 0; i < size; i++) {
            if (strncmp(name, installParam[i].name, strlen(installParam[i].name)) == 0) {
                return CreateCtrlInfo(ctrlInfo, installParam[i].cmd, strlen(name) + 1, 1, "%s.%s", name, value);
            }
        }
    }
    const ParamCmdInfo *other = GetOtherSpecial(&size);
    for (size_t i = 0; i < size; i++) {
        if (strncmp(name, other[i].name, strlen(other[i].name)) == 0) {
            return CreateCtrlInfo(ctrlInfo, other[i].cmd, strlen(other[i].name), 0, "%s.%s", name, value);
        }
    }
    return 0;
}

INIT_LOCAL_API int CheckParameterSet(const char *name,
    const char *value, const ParamSecurityLabel *srcLabel, int *ctrlService)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_LOGV("CheckParameterSet name %s value: %s", name, value);
    PARAM_CHECK(srcLabel != NULL && ctrlService != NULL, return -1, "Invalid param ");
    int ret = CheckParamName(name, 0);
    PARAM_CHECK(ret == 0, return ret, "Illegal param name %s", name);
    ret = CheckParamValue(NULL, name, value, GetParamValueType(name));
    PARAM_CHECK(ret == 0, return ret, "Illegal param value %s", value);
    *ctrlService = 0;

    ServiceCtrlInfo *serviceInfo = NULL;
    GetServiceCtrlInfo(name, value, &serviceInfo);
    ret = CheckParamPermission(srcLabel, (serviceInfo == NULL) ? name : serviceInfo->realKey, DAC_WRITE);
    if (ret == 0) {
        if (serviceInfo == NULL) {
            return 0;
        }
        if (serviceInfo->ctrlParam != 0) {  // ctrl param
            *ctrlService |= PARAM_CTRL_SERVICE;
        }
#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
        // do hook cmd
        PARAM_LOGV("Check parameter settings realKey %s cmd: '%s' value: %s",
            serviceInfo->realKey, serviceInfo->cmdName, (char *)serviceInfo->realKey + serviceInfo->valueOffset);
        DoCmdByName(serviceInfo->cmdName, (char *)serviceInfo->realKey + serviceInfo->valueOffset);
#endif
    }
    if (serviceInfo != NULL) {
        free(serviceInfo);
    }
    return ret;
}

int SystemGetParameterName(ParamHandle handle, char *name, unsigned int len)
{
    PARAM_CHECK(name != NULL && handle != 0, return -1, "The name is null");
    return ReadParamName(handle, name, len);
}

static int AddParam(WorkSpace *workSpace, uint8_t type, const char *name, const char *value, uint32_t *dataIndex)
{
    ParamTrieNode *node = AddTrieNode(workSpace, name, strlen(name));
    PARAM_CHECK(node != NULL, return PARAM_CODE_REACHED_MAX,
        "Failed to add node name %s space %s", name, workSpace->fileName);
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, node->dataIndex);
    if (entry == NULL) {
        uint32_t offset = AddParamNode(workSpace, type, name, strlen(name), value, strlen(value));
        PARAM_CHECK(offset > 0, return PARAM_CODE_REACHED_MAX,
            "Failed to allocate name %s space %s", name, workSpace->fileName);
        SaveIndex(&node->dataIndex, offset);
        long long globalCommitId = ATOMIC_LOAD_EXPLICIT(&workSpace->area->commitId, memory_order_relaxed);
        ATOMIC_STORE_EXPLICIT(&workSpace->area->commitId, ++globalCommitId, memory_order_release);
#ifdef PARAM_SUPPORT_SELINUX
        WorkSpace *space = GetWorkSpace(WORKSPACE_INDEX_DAC);
        if (space != NULL && space != workSpace) { // dac commit is global commit
            globalCommitId = ATOMIC_LOAD_EXPLICIT(&space->area->commitId, memory_order_relaxed);
            ATOMIC_STORE_EXPLICIT(&space->area->commitId, ++globalCommitId, memory_order_release);
        }
#endif
    }
    if (dataIndex != NULL) {
        *dataIndex = node->dataIndex;
    }
    PARAM_LOGV("AddParam name %s value: %s", name, value);
    return 0;
}

static int UpdateParam(const WorkSpace *workSpace, uint32_t *dataIndex, const char *name, const char *value)
{
    ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, *dataIndex);
    PARAM_CHECK(entry != NULL, return PARAM_CODE_REACHED_MAX, "Failed to update param value %s %u", name, *dataIndex);
    PARAM_CHECK(entry->keyLength == strlen(name), return PARAM_CODE_INVALID_NAME, "Failed to check name len %s", name);

    uint32_t valueLen = strlen(value);
    uint32_t commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, memory_order_relaxed);
    ATOMIC_STORE_EXPLICIT(&entry->commitId, commitId | PARAM_FLAGS_MODIFY, memory_order_relaxed);
    long long globalCommitId = ATOMIC_LOAD_EXPLICIT(&workSpace->area->commitId, memory_order_relaxed);
    if (entry->valueLength < PARAM_VALUE_LEN_MAX && valueLen < PARAM_VALUE_LEN_MAX) {
        int ret = ParamMemcpy(entry->data + entry->keyLength + 1, PARAM_VALUE_LEN_MAX, value, valueLen + 1);
        PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_VALUE, "Failed to copy value");
        entry->valueLength = valueLen;
    }
    uint32_t flags = commitId & ~PARAM_FLAGS_COMMITID;
    ATOMIC_STORE_EXPLICIT(&entry->commitId, (++commitId) | flags, memory_order_release);
    ATOMIC_STORE_EXPLICIT(&workSpace->area->commitId, ++globalCommitId, memory_order_release);
#ifdef PARAM_SUPPORT_SELINUX
    WorkSpace *space = GetWorkSpace(WORKSPACE_INDEX_DAC);
    if (space != NULL && space != workSpace) { // dac commit is global commit
        globalCommitId = ATOMIC_LOAD_EXPLICIT(&space->area->commitId, memory_order_relaxed);
        ATOMIC_STORE_EXPLICIT(&space->area->commitId, ++globalCommitId, memory_order_release);
    }
#endif
    PARAM_LOGV("UpdateParam name %s value: %s", name, value);
    futex_wake(&entry->commitId, INT_MAX);
    return 0;
}

INIT_LOCAL_API int WriteParam(const char *name, const char *value, uint32_t *dataIndex, int mode)
{
    PARAM_LOGV("WriteParam %s", name);
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return -1, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Invalid space");
    PARAM_CHECK(value != NULL && name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid name or value");
    WorkSpace *workSpace = GetWorkSpace(GetWorkSpaceIndex(name));
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
    ParamTrieNode *node = FindTrieNode(workSpace, name, strlen(name), NULL);
    int ret = 0;
    if (node != NULL && node->dataIndex != 0) {
        if (dataIndex != NULL) {
            *dataIndex = node->dataIndex;
        }
        if ((mode & LOAD_PARAM_ONLY_ADD) == LOAD_PARAM_ONLY_ADD) {
            return PARAM_CODE_READ_ONLY;
        }
        ParamNode *entry = (ParamNode *)GetTrieNode(workSpace, node->dataIndex);
        PARAM_CHECK(entry != NULL, return PARAM_CODE_REACHED_MAX,
            "Failed to update param value %s %u", name, node->dataIndex);
        // use save type to check value
        ret = CheckParamValue(node, name, value, entry->type);
        PARAM_CHECK(ret == 0, return ret, "Invalid param value param: %s=%s", name, value);
        PARAMSPACE_AREA_RW_LOCK(workSpace);
        ret = UpdateParam(workSpace, &node->dataIndex, name, value);
        PARAMSPACE_AREA_RW_UNLOCK(workSpace);
    } else {
        uint8_t type = GetParamValueType(name);
        ret = CheckParamValue(node, name, value, type);
        PARAM_CHECK(ret == 0, return ret, "Invalid param value param: %s=%s", name, value);
        PARAMSPACE_AREA_RW_LOCK(workSpace);
        ret = AddParam((WorkSpace *)workSpace, type, name, value, dataIndex);
        PARAMSPACE_AREA_RW_UNLOCK(workSpace);
    }
    return ret;
}

INIT_LOCAL_API int CheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    PARAM_CHECK(srcLabel != NULL, return DAC_RESULT_FORBIDED, "The srcLabel is null");
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_WORKSPACE_CHECK(paramSpace, return DAC_RESULT_FORBIDED, "Invalid workspace");
    PARAM_CHECK(paramSpace->checkParamPermission != NULL, return DAC_RESULT_FORBIDED, "Invalid check permission");
    ParamLabelIndex labelIndex = {0};
    labelIndex.selinuxLabelIndex = GetWorkSpaceIndex(name);
    if (labelIndex.selinuxLabelIndex < paramSpace->maxLabelIndex) {
        labelIndex.workspace = paramSpace->workSpace[labelIndex.selinuxLabelIndex];
    }
    PARAM_CHECK(labelIndex.workspace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
#ifndef PARAM_SUPPORT_SELINUX
    (void)FindTrieNode(labelIndex.workspace, name, strlen(name), &labelIndex.dacLabelIndex);
#endif
    PARAM_LOGV("CheckParamPermission %s selinuxLabelIndex: %u %u",
        labelIndex.workspace->fileName, labelIndex.selinuxLabelIndex, labelIndex.dacLabelIndex);
    return paramSpace->checkParamPermission(&labelIndex, srcLabel, name, mode);
}

INIT_LOCAL_API WorkSpace *GetNextWorkSpace(WorkSpace *curr)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return NULL, "Invalid paramSpace");
    uint32_t i = (curr == NULL) ? 0 : (curr->spaceIndex + 1);
    WorkSpace *workSpace = NULL;
    for (; i < paramSpace->maxLabelIndex; ++i) {
        workSpace = GetWorkSpace(i);
        if (workSpace != NULL) {
            return workSpace;
        }
    }
    return NULL;
}

INIT_LOCAL_API uint8_t GetParamValueType(const char *name)
{
    uint32_t labelIndex = 0;
    WorkSpace *space = GetWorkSpace(WORKSPACE_INDEX_DAC);
    if (space == NULL) {
        return PARAM_TYPE_STRING;
    }
    (void)FindTrieNode(space, name, strlen(name), &labelIndex);
    ParamSecurityNode *securityNode = (ParamSecurityNode *)GetTrieNode(space, labelIndex);
    if (securityNode == NULL) {
        return PARAM_TYPE_STRING;
    }
    return securityNode->type;
}

static int CheckParamValueType(const char *name, const char *value, uint8_t paramType)
{
    (void)name;
    if (paramType == PARAM_TYPE_INT) {
        long long int temp1 = 0;
        if (strlen(value) > 1 && value[0] == '-' && StringToLL(value, &temp1) != 0) {
            PARAM_LOGE("Illegal param value %s for int", value);
            return PARAM_CODE_INVALID_VALUE;
        }
        unsigned long long int temp2 = 0;
        if (StringToULL(value, &temp2) != 0) {
            PARAM_LOGE("Illegal param value %s for int", value);
            return PARAM_CODE_INVALID_VALUE;
        }
    } else if (paramType == PARAM_TYPE_BOOL) {
        static const char *validValue[] = {
            "1", "0", "true", "false", "y", "yes", "on", "off", "n", "no"
        };
        size_t i = 0;
        for (; i < ARRAY_LENGTH(validValue); i++) {
            if (strcasecmp(validValue[i], value) == 0) {
                break;
            }
        }
        if (i >= ARRAY_LENGTH(validValue)) {
            PARAM_LOGE("Illegal param value %s for bool", value);
            return PARAM_CODE_INVALID_VALUE;
        }
    }
    return 0;
}

INIT_LOCAL_API int CheckParamValue(const ParamTrieNode *node, const char *name, const char *value, uint8_t paramType)
{
    if (IS_READY_ONLY(name)) {
        PARAM_CHECK(strlen(value) < PARAM_CONST_VALUE_LEN_MAX,
            return PARAM_CODE_INVALID_VALUE, "Illegal param value %s", value);
        if (node != NULL && node->dataIndex != 0) {
            PARAM_LOGE("Read-only param was already set %s", name);
            return PARAM_CODE_READ_ONLY;
        }
    } else {
        PARAM_CHECK(strlen(value) < GetParamMaxLen(paramType),
            return PARAM_CODE_INVALID_VALUE, "Illegal param value %s length", value);
    }
    return CheckParamValueType(name, value, paramType);
}

static int ReadParamName(ParamHandle handle, char *name, uint32_t length)
{
    PARAM_CHECK(name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_WORKSPACE_CHECK(paramSpace, return -1, "Param workspace has not init.");
    PARAM_ONLY_CHECK(handle != (ParamHandle)-1, return PARAM_CODE_NOT_FOUND);
    uint32_t labelIndex = 0;
    uint32_t index = 0;
    PARAM_GET_HANDLE_INFO(handle, labelIndex, index);
    WorkSpace *workSpace = GetWorkSpace(labelIndex);
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_NOT_FOUND, "Invalid workSpace for handle %x", handle);
    ParamNode *entry = NULL;
    if (PARAM_IS_ALIGNED(index)) {
        entry = (ParamNode *)GetTrieNode(workSpace, index);
    }
    if (entry == NULL) {
        return PARAM_CODE_NOT_FOUND;
    }
    PARAM_CHECK(length > entry->keyLength, return -1, "Invalid param size %u %u", entry->keyLength, length);
    int ret = ParamMemcpy(name, length, entry->data, entry->keyLength);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_PARAM, "Failed to copy name");
    name[entry->keyLength] = '\0';
    return 0;
}

void ResetParamSecurityLabel(void)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return, "Invalid paramSpace");
    paramSpace->securityLabel.cred.pid = getpid();
    paramSpace->securityLabel.cred.uid = geteuid();
    paramSpace->securityLabel.cred.gid = getegid();
    paramSpace->flags |= WORKSPACE_FLAGS_NEED_ACCESS;
}