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

#include "param_security.h"
#include "param_utils.h"

#define OCT_BASE 8
#define LABEL "PARAM_DAC"
static ParamSecurityLabel g_localSecurityLabel = {};

static void GetUserIdByName(FILE *fp, uid_t *uid, const char *name, uint32_t nameLen)
{
    *uid = -1;
    (void)fseek(fp, 0, SEEK_SET);
    struct passwd *data = NULL;
    while ((data = fgetpwent(fp)) != NULL) {
        if (strlen(data->pw_name) == nameLen && strncmp(data->pw_name, name, nameLen) == 0) {
            *uid = data->pw_uid;
            return;
        }
    }
}

static void GetGroupIdByName(FILE *fp, gid_t *gid, const char *name, uint32_t nameLen)
{
    *gid = -1;
    (void)fseek(fp, 0, SEEK_SET);
    struct group *data = NULL;
    while ((data = fgetgrent(fp)) != NULL) {
        if (strlen(data->gr_name) == nameLen && strncmp(data->gr_name, name, nameLen) == 0) {
            *gid = data->gr_gid;
            break;
        }
    }
}

// user:group:r|w
static int GetParamDacData(FILE *fpForGroup, FILE *fpForUser, ParamDacData *dacData, const char *value)
{
    char *groupName = strstr(value, ":");
    if (groupName == NULL) {
        return -1;
    }
    char *mode = strstr(groupName + 1, ":");
    if (mode == NULL) {
        return -1;
    }
    GetUserIdByName(fpForUser, &dacData->uid, value, groupName - value);
    GetGroupIdByName(fpForGroup, &dacData->gid, groupName + 1, mode - groupName - 1);
    dacData->mode = strtol(mode + 1, NULL, OCT_BASE);
    return 0;
}

static int InitLocalSecurityLabel(ParamSecurityLabel **security, int isInit)
{
    UNUSED(isInit);
    PARAM_LOGD("InitLocalSecurityLabel uid:%d gid:%d euid: %d egid: %d ", getuid(), getgid(), geteuid(), getegid());
    g_localSecurityLabel.cred.pid = getpid();
    g_localSecurityLabel.cred.uid = geteuid();
    g_localSecurityLabel.cred.gid = getegid();
    *security = &g_localSecurityLabel;
    // support check write permission in client
    (*security)->flags |= LABEL_CHECK_FOR_ALL_PROCESS;
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

static int DecodeSecurityLabel(ParamSecurityLabel **srcLabel, char *buffer, uint32_t bufferSize)
{
    PARAM_CHECK(bufferSize >= sizeof(ParamSecurityLabel), return -1, "Invalid buffersize %u", bufferSize);
    PARAM_CHECK(srcLabel != NULL && buffer != NULL, return -1, "Invalid param");
    *srcLabel = (ParamSecurityLabel *)buffer;
    return 0;
}

static int LoadParamLabels(const char *fileName, SecurityLabelFunc label, void *context)
{
    FILE *fpForGroup = fopen(GROUP_FILE_PATH, "r");
    FILE *fpForUser = fopen(USER_FILE_PATH, "r");
    FILE *fp = fopen(fileName, "r");
    SubStringInfo *info = malloc(sizeof(SubStringInfo) * SUBSTR_INFO_DAC + 1);
    PARAM_CHECK(fpForGroup != NULL && fpForUser != NULL && fp != NULL && info != NULL,
        goto exit, "Can not open file for load param labels");

    uint32_t infoCount = 0;
    char buff[PARAM_BUFFER_SIZE];
    ParamAuditData auditData = {};
    while (fgets(buff, PARAM_BUFFER_SIZE, fp) != NULL) {
        int subStrNumber = GetSubStringInfo(buff, strlen(buff), ' ', info, SUBSTR_INFO_DAC + 1);
        if (subStrNumber <= SUBSTR_INFO_DAC) {
            continue;
        }
        auditData.name = info[SUBSTR_INFO_NAME].value;
#ifdef STARTUP_INIT_TEST
        auditData.label = info[SUBSTR_INFO_NAME].value;
#endif
        int ret = GetParamDacData(fpForGroup, fpForUser, &auditData.dacData, info[SUBSTR_INFO_DAC].value);
        PARAM_CHECK(ret == 0, continue, "Failed to get param info %d %s", ret, buff);
        ret = label(&auditData, context);
        PARAM_CHECK(ret == 0, continue, "Failed to write param info %d %s", ret, buff);
        infoCount++;
    }
    PARAM_LOGI("Load parameter label total %u success %s", infoCount, fileName);
exit:
    if (fp) {
        (void)fclose(fp);
    }
    if (info) {
        free(info);
    }
    if (fpForGroup) {
        (void)fclose(fpForGroup);
    }
    if (fpForUser) {
        (void)fclose(fpForUser);
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
    return ReadFileInDir(path, ".para.dac", ProcessParamFile, &cxt);
}

static int CheckFilePermission(const ParamSecurityLabel *localLabel, const char *fileName, int flags)
{
    UNUSED(flags);
    PARAM_CHECK(localLabel != NULL && fileName != NULL, return -1, "Invalid param");
    return 0;
}

static int CheckParamPermission(const ParamSecurityLabel *srcLabel, const ParamAuditData *auditData, int mode)
{
    int ret = DAC_RESULT_FORBIDED;
    PARAM_CHECK(srcLabel != NULL && auditData != NULL && auditData->name != NULL, return ret, "Invalid param");
    PARAM_CHECK((mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) != 0, return ret, "Invalid mode %x", mode);

    /**
     * DAC group 实现的label的定义
     * user:group:read|write|watch
     */
    uint32_t localMode = 0;
    if (srcLabel->cred.uid == auditData->dacData.uid) {
        localMode = mode & (DAC_READ | DAC_WRITE | DAC_WATCH);
    } else if (srcLabel->cred.gid == auditData->dacData.gid) {
        localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_GROUP_START;
    } else {
        localMode = (mode & (DAC_READ | DAC_WRITE | DAC_WATCH)) >> DAC_OTHER_START;
    }
    if ((auditData->dacData.mode & localMode) != 0) {
        ret = DAC_RESULT_PERMISSION;
    }
    PARAM_LOGD("Src label %d %d ", srcLabel->cred.gid, srcLabel->cred.uid);
    PARAM_LOGD("auditData label %d %d mode %o lable %s",
        auditData->dacData.gid, auditData->dacData.uid, auditData->dacData.mode, auditData->label);
    PARAM_LOGD("%s check %o localMode %o ret %d", auditData->name, mode, localMode, ret);
    return ret;
}

PARAM_STATIC int RegisterSecurityDacOps(ParamSecurityOps *ops, int isInit)
{
    PARAM_CHECK(ops != NULL, return -1, "Invalid param");
    PARAM_LOGI("RegisterSecurityDacOps %d", isInit);
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