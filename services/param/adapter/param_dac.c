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

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "init_utils.h"
#include "param_security.h"
#include "param_utils.h"

#define OCT_BASE 8
static ParamSecurityLabel g_localSecurityLabel = {};

static void GetUserIdByName(uid_t *uid, const char *name, uint32_t nameLen)
{
    *uid = -1;
    struct passwd *data = NULL;
    while ((data = getpwent()) != NULL) {
        if ((data->pw_name != NULL) && (strlen(data->pw_name) == nameLen) &&
            (strncmp(data->pw_name, name, nameLen) == 0)) {
            *uid = data->pw_uid;
            break;
        }
    }
    endpwent();
}

static void GetGroupIdByName(gid_t *gid, const char *name, uint32_t nameLen)
{
    *gid = -1;
    struct group *data = NULL;
    while ((data = getgrent()) != NULL) {
        if ((data->gr_name != NULL) && (strlen(data->gr_name) == nameLen) &&
            (strncmp(data->gr_name, name, nameLen) == 0)) {
            *gid = data->gr_gid;
            break;
        }
    }
    endgrent();
}

// user:group:r|w
static int GetParamDacData(ParamDacData *dacData, const char *value)
{
    if (dacData == NULL) {
        return -1;
    }
    char *groupName = strstr(value, ":");
    if (groupName == NULL) {
        return -1;
    }
    char *mode = strstr(groupName + 1, ":");
    if (mode == NULL) {
        return -1;
    }
    GetUserIdByName(&dacData->uid, value, groupName - value);
    GetGroupIdByName(&dacData->gid, groupName + 1, mode - groupName - 1);
    dacData->mode = strtol(mode + 1, NULL, OCT_BASE);
    return 0;
}

static int InitLocalSecurityLabel(ParamSecurityLabel **security, int isInit)
{
    UNUSED(isInit);
    PARAM_LOGV("InitLocalSecurityLabel uid:%d gid:%d euid: %d egid: %d ", getuid(), getgid(), geteuid(), getegid());
    g_localSecurityLabel.cred.pid = getpid();
    g_localSecurityLabel.cred.uid = geteuid();
    g_localSecurityLabel.cred.gid = getegid();
    *security = &g_localSecurityLabel;
    // support check write permission in client
    (*security)->flags |= LABEL_CHECK_FOR_ALL_PROCESS;
#ifdef WITH_SELINUX
    (*security)->flags = 0;
#endif
    return 0;
}

static int FreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    return 0;
}

static int EncodeSecurityLabel(const ParamSecurityLabel *srcLabel, char *buffer, uint32_t *bufferSize)
{
    PARAM_CHECK(bufferSize != NULL, return -1, "Invalid param");
    if (buffer == NULL) {
        *bufferSize = sizeof(ParamSecurityLabel);
        return 0;
    }
    PARAM_CHECK(*bufferSize >= sizeof(ParamSecurityLabel), return -1, "Invalid buffersize %u", *bufferSize);
    *bufferSize = sizeof(ParamSecurityLabel);
    return memcpy_s(buffer, *bufferSize, srcLabel, sizeof(ParamSecurityLabel));
}

static int DecodeSecurityLabel(ParamSecurityLabel **srcLabel, const char *buffer, uint32_t bufferSize)
{
    PARAM_CHECK(bufferSize >= sizeof(ParamSecurityLabel), return -1, "Invalid buffersize %u", bufferSize);
    PARAM_CHECK(srcLabel != NULL && buffer != NULL, return -1, "Invalid param");
    *srcLabel = (ParamSecurityLabel *)buffer;
    return 0;
}

typedef struct {
    SecurityLabelFunc label;
    void *context;
} LoadContext;

static int LoadOneParam_ (const uint32_t *context, const char *name, const char *value)
{
    LoadContext *loadContext = (LoadContext *)context;
    ParamAuditData auditData = {0};
    auditData.name = name;
#ifdef STARTUP_INIT_TEST
    auditData.label = value;
#endif
    int ret = GetParamDacData(&auditData.dacData, value);
    PARAM_CHECK(ret == 0, return -1, "Failed to get param info %d %s", ret, name);
    ret = loadContext->label(&auditData, loadContext->context);
    PARAM_CHECK(ret == 0, return -1, "Failed to write param info %d \"%s\"", ret, name);
    return 0;
}

static int LoadParamLabels(const char *fileName, SecurityLabelFunc label, void *context)
{
    LoadContext loadContext = {
        label, context
    };
    uint32_t infoCount = 0;
    FILE *fp = fopen(fileName, "r");
    const uint32_t buffSize = PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10; // 10 size
    char *buff = (char *)calloc(1,  buffSize);
    while (fp != NULL && buff != NULL && fgets(buff, buffSize, fp) != NULL) {
        buff[buffSize - 1] = '\0';

        int ret = SpliteString(buff, NULL, 0, LoadOneParam_, (uint32_t *)&loadContext);
        if (ret != 0) {
            PARAM_LOGE("Failed to splite string %s fileName %s", buff, fileName);
            continue;
        }
        infoCount++;
    }
    PARAM_LOGI("Load parameter label total %u success %s", infoCount, fileName);
    if (fp != NULL) {
        (void)fclose(fp);
    }
    if (buff != NULL) {
        free(buff);
    }
    return 0;
}

static int ProcessParamFile(const char *fileName, void *context)
{
    LabelFuncContext *cxt = (LabelFuncContext *)context;
    return LoadParamLabels(fileName, cxt->label, cxt->context);
}

static int GetParamSecurityLabel(SecurityLabelFunc label, const char *path, void *context)
{
    PARAM_CHECK(label != NULL && path != NULL, return -1, "Invalid param");
    struct stat st;
    LabelFuncContext cxt = {label, context};
    if ((stat(path, &st) == 0) && !S_ISDIR(st.st_mode)) {
        return ProcessParamFile(path, &cxt);
    }
    PARAM_LOGV("GetParamSecurityLabel %s ", path);
    return ReadFileInDir(path, ".para.dac", ProcessParamFile, &cxt);
}

static int CheckFilePermission(const ParamSecurityLabel *localLabel, const char *fileName, int flags)
{
    UNUSED(flags);
    PARAM_CHECK(localLabel != NULL && fileName != NULL, return -1, "Invalid param");
    return 0;
}

#ifdef PARAM_SUPPORT_DAC_CHECK
static int CheckUserInGroup(gid_t groupId, uid_t uid)
{
    static char buffer[255] = {0}; // 255 max size
    static char userBuff[255] = {0}; // 255 max size
    struct group *grpResult = NULL;
    struct group grp = {};
    int ret = getgrgid_r(groupId, &grp, buffer, sizeof(buffer), &grpResult);
    if (ret != 0 || grpResult == NULL || grpResult->gr_name == NULL) {
        return -1;
    }
    struct passwd data = {};
    struct passwd *userResult = NULL;
    ret = getpwuid_r(uid, &data, userBuff, sizeof(userBuff), &userResult);
    if (ret != 0 || userResult == NULL || userResult->pw_name == NULL) {
        return -1;
    }

    PARAM_LOGV("CheckUserInGroup pw_name %s ", userResult->pw_name);
    if (strcmp(grpResult->gr_name, userResult->pw_name) == 0) {
        return 0;
    }
    int index = 0;
    while (grpResult->gr_mem[index]) {
        PARAM_LOGV("CheckUserInGroup %s ", grpResult->gr_mem[index]);
        if (strcmp(grpResult->gr_mem[index], userResult->pw_name) == 0) {
            return 0;
        }
        index++;
    }
    return -1;
}
#endif

static int CheckParamPermission(const ParamSecurityLabel *srcLabel, const ParamAuditData *auditData, uint32_t mode)
{
#ifndef PARAM_SUPPORT_DAC_CHECK
    return DAC_RESULT_PERMISSION;
#else
    int ret = DAC_RESULT_FORBIDED;
    PARAM_CHECK(srcLabel != NULL && auditData != NULL && auditData->name != NULL, return ret, "Invalid param");
    PARAM_CHECK((mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) != 0, return ret, "Invalid mode %x", mode);
    if (srcLabel->cred.uid == 0) {
        return DAC_RESULT_PERMISSION;
    }
    /**
     * DAC group 实现的label的定义
     * user:group:read|write|watch
     */
    uint32_t localMode;
    if (srcLabel->cred.uid == auditData->dacData.uid) {
        localMode = mode & (DAC_READ | DAC_WRITE | DAC_WATCH);
    } else if (srcLabel->cred.gid == auditData->dacData.gid) {
        localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_GROUP_START;
    } else if (CheckUserInGroup(auditData->dacData.gid, srcLabel->cred.uid) == 0) {  // user in group
        localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_GROUP_START;
    } else {
        localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_OTHER_START;
    }
    if ((auditData->dacData.mode & localMode) != 0) {
        ret = DAC_RESULT_PERMISSION;
    }
    PARAM_LOGI("Src label gid:%d uid:%d ", srcLabel->cred.gid, srcLabel->cred.uid);
    PARAM_LOGI("local label gid:%d uid:%d mode %o",
        auditData->dacData.gid, auditData->dacData.uid, auditData->dacData.mode);
    PARAM_LOGI("%s check %o localMode %o ret %d", auditData->name, mode, localMode, ret);
    return ret;
#endif
}

PARAM_STATIC int RegisterSecurityDacOps(ParamSecurityOps *ops, int isInit)
{
    PARAM_CHECK(ops != NULL, return -1, "Invalid param");
    PARAM_LOGV("RegisterSecurityDacOps %d", isInit);
    ops->securityGetLabel = NULL;
    ops->securityDecodeLabel = NULL;
    ops->securityEncodeLabel = NULL;
    ops->securityInitLabel = InitLocalSecurityLabel;
    ops->securityCheckFilePermission = CheckFilePermission;
    ops->securityCheckParamPermission = CheckParamPermission;
    ops->securityFreeLabel = FreeLocalSecurityLabel;
    if (isInit) {
        ops->securityGetLabel = GetParamSecurityLabel;
        ops->securityDecodeLabel = DecodeSecurityLabel;
    } else {
        ops->securityEncodeLabel = EncodeSecurityLabel;
    }
    return 0;
}

#ifdef PARAM_SUPPORT_DAC
int RegisterSecurityOps(ParamSecurityOps *ops, int isInit)
{
    return RegisterSecurityDacOps(ops, isInit);
}
#endif
