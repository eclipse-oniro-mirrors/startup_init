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
#include <errno.h>
#include <dlfcn.h>
#include <sys/socket.h>

#include "init_utils.h"
#include "param_manager.h"
#include "param_security.h"
#include "param_utils.h"
#include "param_base.h"
#ifdef PARAM_SUPPORT_SELINUX
#include "selinux_parameter.h"
#endif

#ifdef __aarch64__
#define CHECKER_LIB_NAME "/system/lib64/libparaperm_checker.z.so"
#define CHECKER_UPDATER_LIB "/lib64/libparaperm_checker.z.so"
#else
#define CHECKER_LIB_NAME "/system/lib/libparaperm_checker.z.so"
#define CHECKER_UPDATER_LIB "/lib/libparaperm_checker.z.so"
#endif
typedef int (*SelinuxSetParamCheck)(const char *paraName, const char *destContext, const SrcInfo *info);

static int InitSelinuxOpsForInit(SelinuxSpace *selinuxSpace)
{
    if (selinuxSpace->selinuxHandle == NULL) {
        const char *libname = (GetParamWorkSpace()->ops.updaterMode == 1) ? CHECKER_UPDATER_LIB : CHECKER_LIB_NAME;
        selinuxSpace->selinuxHandle = dlopen(libname, RTLD_LAZY);
        PARAM_CHECK(selinuxSpace->selinuxHandle != NULL,
            return 0, "Failed to dlsym selinuxHandle, %s", dlerror());
    }
    void *handle = selinuxSpace->selinuxHandle;
    if (selinuxSpace->setParamCheck == NULL) {
        selinuxSpace->setParamCheck = (SelinuxSetParamCheck)dlsym(handle, "SetParamCheck");
        PARAM_CHECK(selinuxSpace->setParamCheck != NULL, return -1, "Failed to dlsym setParamCheck %s", dlerror());
    }
    if (selinuxSpace->getParamList == NULL) {
        selinuxSpace->getParamList = (ParamContextsList *(*)()) dlsym(handle, "GetParamList");
        PARAM_CHECK(selinuxSpace->getParamList != NULL, return -1, "Failed to dlsym getParamList %s", dlerror());
    }
    if (selinuxSpace->getParamLabel == NULL) {
        selinuxSpace->getParamLabel = (const char *(*)(const char *))dlsym(handle, "GetParamLabel");
        PARAM_CHECK(selinuxSpace->getParamLabel != NULL, return -1, "Failed to dlsym getParamLabel %s", dlerror());
    }
    if (selinuxSpace->initParamSelinux == NULL) {
        selinuxSpace->initParamSelinux = (int (*)(int))dlsym(handle, "InitParamSelinux");
        PARAM_CHECK(selinuxSpace->initParamSelinux != NULL, return -1, "Failed to dlsym initParamSelinux ");
    }
    if (selinuxSpace->getParamLabelIndex == NULL) {
        selinuxSpace->getParamLabelIndex = (int (*)(const char *))dlsym(handle, "GetParamLabelIndex");
        PARAM_CHECK(selinuxSpace->getParamLabelIndex != NULL, return -1, "Failed to dlsym getParamLabelIndex ");
    }
    if (selinuxSpace->setSelinuxLogCallback == NULL) {
        selinuxSpace->setSelinuxLogCallback = (void (*)())dlsym(handle, "SetInitSelinuxLog");
    }
    if (selinuxSpace->destroyParamList == NULL) {
        selinuxSpace->destroyParamList =
            (void (*)(ParamContextsList **))dlsym(handle, "DestroyParamList");
        PARAM_CHECK(selinuxSpace->destroyParamList != NULL,
            return -1, "Failed to dlsym destroyParamList %s", dlerror());
    }

    // init and open avc log
    int ret = selinuxSpace->initParamSelinux(1);
    if (selinuxSpace->setSelinuxLogCallback != NULL) {
        selinuxSpace->setSelinuxLogCallback();
    }
    return ret;
}

static int InitLocalSecurityLabel(ParamSecurityLabel *security, int isInit)
{
    PARAM_CHECK(GetParamWorkSpace() != NULL, return -1, "Invalid workspace");
    UNUSED(isInit);
    PARAM_CHECK(security != NULL, return -1, "Invalid security");
    security->cred.pid = getpid();
    security->cred.uid = geteuid();
    security->cred.gid = getegid();
    security->flags[PARAM_SECURITY_SELINUX] = 0;
    PARAM_LOGV("InitLocalSecurityLabel");
#if !(defined STARTUP_INIT_TEST || defined LOCAL_TEST)
    if ((bool)isInit) {
        int ret = InitSelinuxOpsForInit(&GetParamWorkSpace()->selinuxSpace);
        PARAM_CHECK(ret == 0, return -1, "Failed to init selinux ops");
    } else {
        SelinuxSpace *selinuxSpace = &GetParamWorkSpace()->selinuxSpace;
        selinuxSpace->initParamSelinux = InitParamSelinux;
        selinuxSpace->getParamList = GetParamList;
        selinuxSpace->getParamLabel = GetParamLabel;
        selinuxSpace->destroyParamList = DestroyParamList;
        selinuxSpace->getParamLabelIndex = GetParamLabelIndex;
        // init
        selinuxSpace->initParamSelinux(isInit);
    }
#endif
    PARAM_LOGV("Load selinux lib success.");
    return 0;
}

static int FreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    return 0;
}

static void SetSelinuxFileCon(const char *name, const char *context)
{
    PARAM_CHECK(GetParamWorkSpace() != NULL && GetParamWorkSpace()->ops.setfilecon != NULL,
        return, "Invalid workspace or setfilecon");
    static char buffer[FILENAME_LEN_MAX] = {0};
    int len = ParamSprintf(buffer, sizeof(buffer), "%s/%s", PARAM_STORAGE_PATH, context);
    if (len > 0) {
        buffer[len] = '\0';
        PARAM_LOGV("setfilecon name %s path: %s %s ", name, context, buffer);
        if (GetParamWorkSpace()->ops.setfilecon(buffer, context) < 0) {
            PARAM_LOGE("Failed to setfilecon %s ", context);
        }
    }
}

static uint32_t GetWorkSpaceSize(const char *content)
{
    if (strcmp(content, WORKSPACE_NAME_DEF_SELINUX) == 0) {
        return PARAM_WORKSPACE_MAX;
    }
    char name[PARAM_NAME_LEN_MAX] = {0};
    int index = 6; // 6 strlen for const.
    (void)ParamMemcpy(name, sizeof(name), "const.", index);
    size_t len = strlen(content);
    for (size_t i = strlen("u:object_r:"); i < len; i++) {
        if (*(content + i) == ':') {
            break;
        }
        name[index++] = *(content + i);
    }
    if (index == 0) {
#ifdef STARTUP_INIT_TEST
        return PARAM_WORKSPACE_DEF;
#else
        return PARAM_WORKSPACE_MIN;
#endif
    }
    ParamNode *node = GetParamNode(WORKSPACE_INDEX_BASE, name);
    if (node == NULL) {
#ifdef STARTUP_INIT_TEST
        return PARAM_WORKSPACE_DEF;
#else
        return PARAM_WORKSPACE_MIN;
#endif
    }
    int ret = ParamMemcpy(name, sizeof(name) - 1, node->data + node->keyLength + 1, node->valueLength);
    if (ret == 0) {
        name[node->valueLength] = '\0';
        errno = 0;
        uint32_t value = (uint32_t)strtoul(name, NULL, DECIMAL_BASE);
        return (errno != 0) ? PARAM_WORKSPACE_MIN : value;
    }
    return PARAM_WORKSPACE_MIN;
}

static void HandleSelinuxLabelForInit(const ParameterNode *paramNode, int readOnly)
{
    uint32_t labelIndex = paramNode->index + WORKSPACE_INDEX_BASE;
    int ret = AddWorkSpace(paramNode->paraContext, labelIndex, readOnly, GetWorkSpaceSize(paramNode->paraContext));
    if (ret != 0) {
        PARAM_LOGE("Forbid to add selinux workspace %s %s", paramNode->paraName, paramNode->paraContext);
        return;
    }
    // set selinux label
    SetSelinuxFileCon(paramNode->paraName, paramNode->paraContext);
}

static int SelinuxGetAllLabel(int readOnly,
    void (*handleSelinuxLabel)(const ParameterNode *paramNode, int readOnly))
{
    SelinuxSpace *selinuxSpace = &GetParamWorkSpace()->selinuxSpace;
    PARAM_CHECK(selinuxSpace->getParamList != NULL, return DAC_RESULT_FORBIDED, "Invalid getParamList");
    ParamContextsList *node = selinuxSpace->getParamList();
    int count = 0;
    while (node != NULL) {
        PARAM_LOGV("SelinuxGetAllLabel index %d name %s content %s",
            node->info.index, node->info.paraName, node->info.paraContext);
        if (node->info.paraContext == NULL || node->info.paraName == NULL) {
            node = node->next;
            continue;
        }
        handleSelinuxLabel(&node->info, readOnly);
        count++;
        node = node->next;
    }
    ParameterNode tmpNode = {WORKSPACE_NAME_DEF_SELINUX, WORKSPACE_NAME_DEF_SELINUX, 0};
    handleSelinuxLabel(&tmpNode, readOnly);
    PARAM_LOGV("Selinux get all label counts %d.", count);
    return 0;
}

static void HandleSelinuxLabelForPermission(const ParameterNode *paramNode, int readOnly)
{
    uint32_t labelIndex = paramNode->index + WORKSPACE_INDEX_BASE;
    if (labelIndex == WORKSPACE_INDEX_BASE) {
        return;
    }
    if (*(paramNode->paraName + strlen(paramNode->paraName) - 1) != '.') {
        return;
    }
    // save selinux index
    ParamWorkSpace *paramWorkspace = GetParamWorkSpace();
    PARAM_CHECK(paramWorkspace != NULL && paramWorkspace->workSpace != NULL, return, "Invalid workspace");
    WorkSpace *space = paramWorkspace->workSpace[WORKSPACE_INDEX_DAC];
    PARAM_CHECK(space != NULL && space->area != NULL, return, "Failed to get dac space %s", paramNode->paraName);
    uint32_t index = 0;
    (void)FindTrieNode(space, paramNode->paraName, strlen(paramNode->paraName), &index);
    ParamSecurityNode *node = (ParamSecurityNode *)GetTrieNode(space, index);
    PARAM_CHECK(node != NULL, return, "Can not get security label for %s", paramNode->paraName);
    PARAM_LOGV("HandleSelinuxLabelForPermission %s selinuxIndex [ %u %u] dac %u %s ",
        paramNode->paraName, labelIndex, node->selinuxIndex, index, paramNode->paraContext);
    ParamAuditData auditData = {0};
    auditData.dacData.gid = node->gid;
    auditData.dacData.uid = node->uid;
    auditData.dacData.mode = node->mode;
    auditData.dacData.paramType = node->type;
    auditData.selinuxIndex = labelIndex;
    auditData.name = paramNode->paraName;
    AddSecurityLabel(&auditData);
}

static int SelinuxGetParamSecurityLabel(const char *path)
{
    UNUSED(path);
    if (path == NULL) {
        return SelinuxGetAllLabel(0, HandleSelinuxLabelForInit);
    }
    return SelinuxGetAllLabel(0, HandleSelinuxLabelForPermission);
}

static int CheckFilePermission(const ParamSecurityLabel *localLabel, const char *fileName, int flags)
{
    UNUSED(flags);
    UNUSED(localLabel);
    UNUSED(fileName);
    return 0;
}

static void HandleSelinuxLabel(const ParameterNode *node, int readOnly)
{
    PARAM_LOGV("HandleSelinuxLabel %s %s", node->paraName, node->paraContext);
    uint32_t index = node->index + WORKSPACE_INDEX_BASE;
    int ret = AddWorkSpace(node->paraContext, index, readOnly, GetWorkSpaceSize(node->paraContext));
    if (ret != 0) {
        PARAM_LOGE("Forbid to add selinux workspace %s %s", node->paraName, node->paraContext);
    }
    if (readOnly == 2) { // 2 mean need to open workspace
        ret = OpenWorkSpace(index, 1);
        if (ret != 0) {
            PARAM_LOGE("Failed to open selinux workspace %s %s index %u", node->paraName, node->paraContext, index);
        }
    }
}

static int UpdaterCheckParamPermission(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    return DAC_RESULT_PERMISSION;
}

static int OpenPermissionWorkSpace(const char *path)
{
    static int loadLabels = 0;
    int ret = 0;
    if (path == NULL) {
        ret = SelinuxGetAllLabel(1, HandleSelinuxLabel);
    } else if (strcmp(path, "open") == 0) {
        if (loadLabels == 0) {
            ret = SelinuxGetAllLabel(2, HandleSelinuxLabel); // 2 mean need to open workspace
        }
        loadLabels = 1;
    }
    return ret;
}

INIT_LOCAL_API int RegisterSecuritySelinuxOps(ParamSecurityOps *ops, int isInit)
{
    PARAM_CHECK(GetParamWorkSpace() != NULL, return -1, "Invalid workspace");
    PARAM_CHECK(ops != NULL, return -1, "Invalid param");
    int ret = ParamStrCpy(ops->name, sizeof(ops->name), "selinux");
    ops->securityGetLabel = NULL;
    ops->securityInitLabel = InitLocalSecurityLabel;
    ops->securityCheckFilePermission = CheckFilePermission;
    if (GetParamWorkSpace()->ops.updaterMode == 1) {
        ops->securityCheckParamPermission = UpdaterCheckParamPermission;
    } else {
#ifdef STARTUP_INIT_TEST
        ops->securityCheckParamPermission = SelinuxCheckParamPermission;
#endif
    }
    ops->securityFreeLabel = FreeLocalSecurityLabel;
    if (isInit != 0) {
        ops->securityGetLabel = SelinuxGetParamSecurityLabel;
    } else {
        ops->securityGetLabel = OpenPermissionWorkSpace;
    }
    return ret;
}
