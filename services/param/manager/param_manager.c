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
#include <inttypes.h>
#include <limits.h>

#include "init_cmds.h"
#include "init_hook.h"
#include "param_base.h"
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

    WorkSpace *workspace = GetWorkSpaceByName(name);
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
    ATOMIC_SYNC_OR_AND_FETCH(&param->commitId, PARAM_FLAGS_WAITED, MEMORY_ORDER_RELEASE);
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
            PARAM_DUMP("\tparameter length info [%d] [%u, %u] \n\t  param: %s \n",
                entry->commitId, entry->keyLength, entry->valueLength, entry->data);
        }
    }
    if (current->labelIndex == 0) {
        return 0;
    }
    ParamSecurityNode *label = (ParamSecurityNode *)GetTrieNode(workSpace, current->labelIndex);
    if (label == NULL) {
        return 0;
    }
    PARAM_DUMP("\tparameter label dac %u %u 0%o \n", label->uid, label->gid, label->mode);
    PARAM_DUMP("\tparameter label dac member [%u] ", label->memberNum);
    for (uint32_t i = 0; i < label->memberNum; i++) {
        PARAM_DUMP(" %u", label->members[i]);
    }
    if (label->memberNum > 0) {
        PARAM_DUMP("\n");
    }
    return 0;
}

static void HashNodeTraverseForDump(WorkSpace *workSpace, int verbose)
{
    PARAM_DUMP("    map file: %s \n", workSpace->fileName);
    PARAM_DUMP("    space index : %d \n", workSpace->spaceIndex);
    PARAM_DUMP("    space size  : %d \n", workSpace->spaceSize);
    if (workSpace->area != NULL) {
        PARAM_DUMP("    total size: %u \n", workSpace->area->dataSize);
        PARAM_DUMP("    first offset: %u \n", workSpace->area->firstNode);
        PARAM_DUMP("    current offset: %u \n", workSpace->area->currOffset);
        PARAM_DUMP("    total node: %u \n", workSpace->area->trieNodeCount);
        PARAM_DUMP("    total param node: %u \n", workSpace->area->paramNodeCount);
        PARAM_DUMP("    total security node: %u\n", workSpace->area->securityNodeCount);
        if (verbose) {
            PARAM_DUMP("    commitId        : %" PRId64 "\n", workSpace->area->commitId);
            PARAM_DUMP("    commitPersistId : %" PRId64 "\n", workSpace->area->commitPersistId);
        }
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
    // show workspace size
    WorkSpaceSize *spaceSize = GetWorkSpaceSize(GetWorkSpace(WORKSPACE_INDEX_SIZE));
    if (spaceSize != NULL) {
        PARAM_DUMP("Max label index : %u\n", spaceSize->maxLabelIndex);
        for (uint32_t i = 0; i < spaceSize->maxLabelIndex; i++) {
            if (spaceSize->spaceSize[i] == PARAM_WORKSPACE_MIN) {
                continue;
            }
            PARAM_DUMP("\t workspace %u size : %u\n", i, spaceSize->spaceSize[i]);
        }
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

#ifdef PARAM_SUPPORT_SELINUX // load security label
    ParamSecurityOps *ops = GetParamSecurityOps(PARAM_SECURITY_SELINUX);
    if (ops != NULL && ops->securityGetLabel != NULL) {
        ops->securityGetLabel("open");
    }
#endif
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
    PARAM_CHECK(*ctrlInfo != NULL, return -1, "Failed to alloc memory %s", cmd);
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
            return CreateCtrlInfo(ctrlInfo, other[i].cmd, strlen(other[i].replace), 0, "%s.%s", name, value);
        }
    }
    return 0;
}

INIT_LOCAL_API int CheckParameterSet(const char *name,
    const char *value, const ParamSecurityLabel *srcLabel, int *ctrlService)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return PARAM_WORKSPACE_NOT_INIT, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return PARAM_WORKSPACE_NOT_INIT, "Invalid space");
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
        int cmdIndex = 0;
        (void)GetMatchCmd(serviceInfo->cmdName, &cmdIndex);
        DoCmdByIndex(cmdIndex, (char *)serviceInfo->realKey + serviceInfo->valueOffset, NULL);
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
        ATOMIC_SYNC_ADD_AND_FETCH(&workSpace->area->commitId, 1, MEMORY_ORDER_RELEASE);
#ifdef PARAM_SUPPORT_SELINUX
        WorkSpace *space = GetWorkSpace(WORKSPACE_INDEX_DAC);
        if (space != NULL && space != workSpace) { // dac commit is global commit
            ATOMIC_SYNC_ADD_AND_FETCH(&space->area->commitId, 1, MEMORY_ORDER_RELEASE);
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
    uint32_t commitId = ATOMIC_LOAD_EXPLICIT(&entry->commitId, MEMORY_ORDER_RELAXED);
    ATOMIC_STORE_EXPLICIT(&entry->commitId, commitId | PARAM_FLAGS_MODIFY, MEMORY_ORDER_RELAXED);
    if (entry->valueLength < PARAM_VALUE_LEN_MAX && valueLen < PARAM_VALUE_LEN_MAX) {
        int ret = PARAM_MEMCPY(entry->data + entry->keyLength + 1, PARAM_VALUE_LEN_MAX, value, valueLen + 1);
        PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_VALUE, "Failed to copy value");
        entry->valueLength = valueLen;
    }
    uint32_t flags = commitId & ~PARAM_FLAGS_COMMITID;
    ATOMIC_STORE_EXPLICIT(&entry->commitId, (++commitId) | flags, MEMORY_ORDER_RELEASE);
    ATOMIC_SYNC_ADD_AND_FETCH(&workSpace->area->commitId, 1, MEMORY_ORDER_RELEASE);
#ifdef PARAM_SUPPORT_SELINUX
    WorkSpace *space = GetWorkSpace(WORKSPACE_INDEX_DAC);
    if (space != NULL && space != workSpace) { // dac commit is global commit
        ATOMIC_SYNC_ADD_AND_FETCH(&space->area->commitId, 1, MEMORY_ORDER_RELEASE);
    }
#endif
    PARAM_LOGV("UpdateParam name %s value: %s", name, value);
    futex_wake(&entry->commitId, INT_MAX);
    return 0;
}

INIT_LOCAL_API int WriteParam(const char *name, const char *value, uint32_t *dataIndex, int mode)
{
    int flag = 0;
    PARAM_LOGV("WriteParam %s", name);
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(paramSpace != NULL, return PARAM_WORKSPACE_NOT_INIT, "Invalid paramSpace");
    PARAM_WORKSPACE_CHECK(paramSpace, return PARAM_WORKSPACE_NOT_INIT, "Invalid space");
    PARAM_CHECK(value != NULL && name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid name or value");
    WorkSpace *workSpace = GetWorkSpaceByName(name);
    PARAM_CHECK(workSpace != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid workSpace");
#ifdef PARAM_SUPPORT_SELINUX
    if (strcmp(workSpace->fileName, WORKSPACE_NAME_DEF_SELINUX) == 0) {
        flag = 1;
    }
#endif
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
    if ((ret == PARAM_CODE_REACHED_MAX) && (flag == 1)) {
        PARAM_LOGE("Add node %s to space %s failed! memory is not enough, system reboot!", name, workSpace->fileName);
        return PARAM_DEFAULT_PARAM_MEMORY_NOT_ENOUGH;
    }
    return ret;
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
    int ret = PARAM_MEMCPY(name, length, entry->data, entry->keyLength);
    PARAM_CHECK(ret == 0, return PARAM_CODE_INVALID_PARAM, "Failed to copy name");
    name[entry->keyLength] = '\0';
    return 0;
}

INIT_LOCAL_API int CheckParamName(const char *name, int info)
{
    PARAM_CHECK(name != NULL, return PARAM_CODE_INVALID_PARAM, "Invalid param");
    size_t nameLen = strlen(name);
    if (nameLen >= PARAM_NAME_LEN_MAX) {
        return PARAM_CODE_INVALID_NAME;
    }
    if (strcmp(name, "#") == 0) {
        return 0;
    }

    if (nameLen < 1 || name[0] == '.' || (!info && name[nameLen - 1] == '.')) {
        PARAM_LOGE("CheckParamName %s %d", name, info);
        return PARAM_CODE_INVALID_NAME;
    }

    /* Only allow alphanumeric, plus '.', '-', '@', ':', or '_' */
    /* Don't allow ".." to appear in a param name */
    for (size_t i = 0; i < nameLen; i++) {
        if (name[i] == '.') {
            if (name[i - 1] == '.') {
                return PARAM_CODE_INVALID_NAME;
            }
            continue;
        }
        if (name[i] == '_' || name[i] == '-' || name[i] == '@' || name[i] == ':') {
            continue;
        }
        if (isalnum(name[i])) {
            continue;
        }
        return PARAM_CODE_INVALID_NAME;
    }
    return 0;
}

static int CheckParamPermission_(WorkSpace **workspace, ParamTrieNode **node,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    PARAM_CHECK(srcLabel != NULL, return DAC_RESULT_FORBIDED, "The srcLabel is null");
    WorkSpace *dacSpace = GetWorkSpace(WORKSPACE_INDEX_DAC);
    PARAM_CHECK(paramSpace->checkParamPermission != NULL, return DAC_RESULT_FORBIDED, "Invalid check permission");
    ParamLabelIndex labelIndex = {0};
    // search node from dac space, and get selinux label index
    *node = FindTrieNode(dacSpace, name, strlen(name), &labelIndex.dacLabelIndex);
    labelIndex.workspace = GetWorkSpaceByName(name);
    PARAM_CHECK(labelIndex.workspace != NULL, return DAC_RESULT_FORBIDED, "Invalid workSpace for %s", name);
    labelIndex.selinuxLabelIndex = labelIndex.workspace->spaceIndex;

    int ret = paramSpace->checkParamPermission(&labelIndex, srcLabel, name, mode);
    PARAM_CHECK(ret == 0, return ret,
        "Forbid to access %s label %u %u", name, labelIndex.dacLabelIndex, labelIndex.selinuxLabelIndex);
    *workspace = labelIndex.workspace;
    return ret;
}

INIT_LOCAL_API int CheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    ParamTrieNode *entry = NULL;
    WorkSpace *workspace = NULL;
    return CheckParamPermission_(&workspace, &entry, srcLabel, name, mode);
}

STATIC_INLINE ParamTrieNode *GetTrieNodeByHandle(ParamHandle handle)
{
    PARAM_ONLY_CHECK(handle != (ParamHandle)-1, return NULL);
    uint32_t labelIndex = 0;
    uint32_t index = 0;
    PARAM_GET_HANDLE_INFO(handle, labelIndex, index);
    WorkSpace *workSpace = GetWorkSpace(labelIndex);
    PARAM_CHECK(workSpace != NULL, return NULL, "Invalid workSpace for handle %x", handle);
    if (PARAM_IS_ALIGNED(index)) {
        return (ParamTrieNode *)GetTrieNode(workSpace, index);
    }
    return NULL;
}

STATIC_INLINE int ReadParamValue(ParamNode *entry, char *value, uint32_t *length)
{
    if (entry == NULL) {
        return PARAM_CODE_NOT_FOUND;
    }
    if (value == NULL) {
        *length = entry->valueLength + 1;
        return 0;
    }
    PARAM_CHECK(*length > entry->valueLength, return PARAM_CODE_INVALID_VALUE,
        "Invalid value len %u %u", *length, entry->valueLength);
    uint32_t commitId = ReadCommitId(entry);
    return ReadParamValue_(entry, &commitId, value, length);
}

int SystemReadParam(const char *name, char *value, uint32_t *len)
{
    PARAM_WORKSPACE_CHECK(GetParamWorkSpace(), return PARAM_WORKSPACE_NOT_INIT,
        "SystemReadParam failed! name is:%s, errNum is:%d!", name, PARAM_WORKSPACE_NOT_INIT);
    PARAM_CHECK(name != NULL && len != NULL, return PARAM_CODE_ERROR,
        "SystemReadParam failed! name is:%s, errNum is:%d!", name, PARAM_CODE_ERROR);
    ParamTrieNode *node = NULL;
    WorkSpace *workspace = NULL;
    int ret = CheckParamPermission_(&workspace, &node, GetParamSecurityLabel(), name, DAC_READ);
    if (ret != 0) {
        PARAM_LOGE("SystemReadParam failed! name is:%s, errNum is:%d!", name, ret);
        return ret;
    }
#ifdef PARAM_SUPPORT_SELINUX
    // search from real workspace
    node = FindTrieNode(workspace, name, strlen(name), NULL);
#endif
    if (node == NULL) {
        return PARAM_CODE_NOT_FOUND;
    }
    ret =  ReadParamValue((ParamNode *)GetTrieNode(workspace, node->dataIndex), value, len);
    if (ret != 0) {
        PARAM_LOGE("SystemReadParam failed! name is:%s, errNum is:%d!", name, ret);
    }
    return ret;
}

int SystemFindParameter(const char *name, ParamHandle *handle)
{
    PARAM_WORKSPACE_CHECK(GetParamWorkSpace(), return PARAM_WORKSPACE_NOT_INIT, "Param workspace has not init.");
    PARAM_CHECK(name != NULL && handle != NULL, return -1, "The name or handle is null");
    *handle = -1;
    ParamTrieNode *entry = NULL;
    WorkSpace *workspace = NULL;
    int ret = CheckParamPermission_(&workspace, &entry, GetParamSecurityLabel(), name, DAC_READ);
    PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);
#ifdef PARAM_SUPPORT_SELINUX
    // search from real workspace
    entry = FindTrieNode(workspace, name, strlen(name), NULL);
#endif
    if (entry != NULL && entry->dataIndex != 0) {
        *handle = PARAM_HANDLE(workspace, entry->dataIndex);
        return 0;
    } else if (entry != NULL) {
        return PARAM_CODE_NODE_EXIST;
    }
    return PARAM_CODE_NOT_FOUND;
}

int SystemGetParameterCommitId(ParamHandle handle, uint32_t *commitId)
{
    PARAM_WORKSPACE_CHECK(GetParamWorkSpace(), return -1, "Param workspace has not init.");
    PARAM_ONLY_CHECK(handle != (ParamHandle)-1, return PARAM_CODE_NOT_FOUND);
    PARAM_CHECK(handle != 0 && commitId != NULL, return -1, "The handle is null");
    ParamNode *entry = (ParamNode *)GetTrieNodeByHandle(handle);
    if (entry == NULL) {
        return PARAM_CODE_NOT_FOUND;
    }
    *commitId = ReadCommitId(entry);
    return 0;
}

long long GetSystemCommitId(void)
{
    PARAM_WORKSPACE_CHECK(GetParamWorkSpace(), return -1, "Param workspace has not init.");
    WorkSpace *space = GetWorkSpace(WORKSPACE_INDEX_DAC);
    if (space == NULL || space->area == NULL) {
        return 0;
    }
    return ATOMIC_UINT64_LOAD_EXPLICIT(&space->area->commitId, MEMORY_ORDER_ACQUIRE);
}

int SystemGetParameterValue(ParamHandle handle, char *value, unsigned int *len)
{
    PARAM_WORKSPACE_CHECK(GetParamWorkSpace(), return -1, "Param workspace has not init.");
    PARAM_ONLY_CHECK(handle != (ParamHandle)-1, return PARAM_CODE_NOT_FOUND);
    PARAM_CHECK(len != NULL && handle != 0, return -1, "The value is null");
    return ReadParamValue((ParamNode *)GetTrieNodeByHandle(handle), value, len);
}

INIT_LOCAL_API int CheckIfUidInGroup(const gid_t groupId, const char *groupCheckName)
{
    PARAM_CHECK(groupCheckName != NULL, return -1, "Invalid groupCheckName");
    struct group *groupName = getgrnam(groupCheckName);
    PARAM_CHECK(groupName != NULL, return -1, "Not find %s group", groupCheckName);
    char  **gr_mem = groupName->gr_mem;
    PARAM_CHECK(gr_mem != NULL, return -1, "No member in %s", groupCheckName);
    for (int i = 0; gr_mem[i] != NULL; ++i) {
        struct group *userGroup = getgrnam(gr_mem[i]);
        if (userGroup != NULL) {
            if (groupId == userGroup->gr_gid) {
                return 0;
            }
        }
    }
    PARAM_LOGE("Forbid to access, groupId %u not in %s", groupId, groupCheckName);
    return PARAM_CODE_PERMISSION_DENIED;
}
