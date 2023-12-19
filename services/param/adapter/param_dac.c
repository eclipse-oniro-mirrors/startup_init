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
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

#include "param_manager.h"
#include "param_security.h"
#include "param_trie.h"
#include "param_utils.h"
#include "param_base.h"

#define MAX_MEMBER_IN_GROUP  128
#define MAX_BUF_SIZE  1024
#define INVALID_MODE 0550
#ifdef STARTUP_INIT_TEST
#define GROUP_FILE_PATH STARTUP_INIT_UT_PATH "/etc/group"
#else
#define GROUP_FILE_PATH "/etc/group"
#endif
#define OCT_BASE 8
#define INVALID_UID(uid) ((uid) == (uid_t)-1)

static void GetUserIdByName(uid_t *uid, const char *name)
{
    struct passwd *data = getpwnam(name);
    if (data == NULL) {
        *uid = -1;
        return ;
    }
    *uid = data->pw_uid;
}

static void GetGroupIdByName(gid_t *gid, const char *name)
{
    *gid = -1;
    struct group *data = getgrnam(name);
    if (data != NULL) {
        *gid = data->gr_gid;
        return;
    }
    while ((data = getgrent()) != NULL) {
        if ((data->gr_name != NULL) && (strcmp(data->gr_name, name) == 0)) {
            *gid = data->gr_gid;
            break;
        }
    }
    endgrent();
}

// user:group:r|w
static int GetParamDacData(ParamDacData *dacData, const char *value)
{
    static const struct {
        const char *name;
        int value;
    } paramTypes[] = {
        { "int", PARAM_TYPE_INT },
        { "string", PARAM_TYPE_STRING },
        { "bool", PARAM_TYPE_BOOL },
    };

    char *groupName = strstr(value, ":");
    if (groupName == NULL) {
        return -1;
    }
    char *mode = strstr(groupName + 1, ":");
    if (mode == NULL) {
        return -1;
    }

    uint32_t nameLen = groupName - value;
    char *name = (char *)value;
    name[nameLen] = '\0';
    GetUserIdByName(&dacData->uid, name);
    nameLen = mode - groupName - 1;
    name = (char *)groupName + 1;
    name[nameLen] = '\0';
    GetGroupIdByName(&dacData->gid, name);

    dacData->paramType = PARAM_TYPE_STRING;
    char *type = strstr(mode + 1, ":");
    if (type != NULL) {
        *type = '\0';
        type++;
        for (size_t i = 0; (type != NULL) && (i < ARRAY_LENGTH(paramTypes)); i++) {
            if (strcmp(paramTypes[i].name, type) == 0) {
                dacData->paramType = paramTypes[i].value;
            }
        }
    }
    dacData->mode = (uint16_t)strtol(mode + 1, NULL, OCT_BASE);
    return 0;
}

static int InitLocalSecurityLabel(ParamSecurityLabel *security, int isInit)
{
    UNUSED(isInit);
    PARAM_CHECK(security != NULL, return -1, "Invalid security");
    security->cred.pid = getpid();
    security->cred.uid = geteuid();
    security->cred.gid = getegid();
    // support check write permission in client
    security->flags[PARAM_SECURITY_DAC] |= LABEL_CHECK_IN_ALL_PROCESS;
    return 0;
}

static int FreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    return 0;
}

static int DacGetGroupMember(gid_t gid, uid_t *member, uint32_t *memberSize)
{
    uint32_t inputLen = *memberSize;
    *memberSize = 0;
    struct group *data = getgrgid(gid);
    if (data == NULL || data->gr_mem == NULL) {
        return 0;
    }
    int i = 0;
    int memIndex = 0;
    while (data->gr_mem[i]) {
        uid_t uid;
        GetUserIdByName(&uid, data->gr_mem[i]);
        if (INVALID_UID(uid)) {
            i++;
            continue;
        }
        if ((memIndex + 1) > inputLen) {
            PARAM_LOGE("Not enough memory for uid member %u", gid);
            break;
        }
        member[memIndex++] = uid;
        i++;
    }
    uid_t uid = 0;
    GetUserIdByName(&uid, data->gr_name);
    if (!INVALID_UID(uid) && ((memIndex + 1) < inputLen)) {
        member[memIndex++] = uid;
    }
    *memberSize = memIndex;
    return 0;
}

static int LoadOneParam_(const uint32_t *context, const char *name, const char *value)
{
    ParamAuditData *auditData = (ParamAuditData *)context;
    auditData->dacData.gid = -1;
    auditData->dacData.uid = -1;
    auditData->name = name;
    int ret = GetParamDacData(&auditData->dacData, value);
    PARAM_CHECK(ret == 0, return -1, "Failed to get param info %d %s", ret, name);
    if (INVALID_UID(auditData->dacData.gid) || INVALID_UID(auditData->dacData.uid)) {
        PARAM_LOGW("Invalid dac for '%s' gid %d uid %d", name, auditData->dacData.gid, auditData->dacData.uid);
    }
    // get uid from group
    auditData->memberNum = MAX_MEMBER_IN_GROUP;
    ret = DacGetGroupMember(auditData->dacData.gid, auditData->members, &auditData->memberNum);
    if (ret != 0) {
        auditData->memberNum = 1;
        auditData->members[0] = auditData->dacData.gid;
    }

    return AddSecurityLabel(auditData);
}

static int LoadParamLabels(const char *fileName)
{
    int result = 0;
    ParamAuditData *auditData = (ParamAuditData *)calloc(1,
        sizeof(ParamAuditData) + sizeof(uid_t) * MAX_MEMBER_IN_GROUP);
    if (auditData == NULL) {
        PARAM_LOGE("Failed to alloc memory %s", fileName);
        return result;
    }
    uint32_t infoCount = 0;
    FILE *fp = fopen(fileName, "r");
    const uint32_t buffSize = PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX + 10;  // 10 size
    char *buff = (char *)calloc(1, buffSize);
    while (fp != NULL && buff != NULL && fgets(buff, buffSize, fp) != NULL) {
        buff[buffSize - 1] = '\0';
        result = SplitParamString(buff, NULL, 0, LoadOneParam_, (const uint32_t *)auditData);
        if (result != 0) {
            PARAM_LOGE("Failed to split string %s fileName %s, result is:%d", buff, fileName, result);
            break;
        }
        infoCount++;
    }

    if (result == 0) {
        PARAM_LOGI("Load parameter label total %u success %s", infoCount, fileName);
    }

    if (fp != NULL) {
        (void)fclose(fp);
    }
    if (buff != NULL) {
        free(buff);
    }
    if (auditData != NULL) {
        free(auditData);
    }
    return result;
}

static int ProcessParamFile(const char *fileName, void *context)
{
    UNUSED(context);
    return LoadParamLabels(fileName);
}

static int DacGetParamSecurityLabel(const char *path)
{
    PARAM_CHECK(path != NULL, return -1, "Invalid param");
    struct stat st;
    if ((stat(path, &st) == 0) && !S_ISDIR(st.st_mode)) {
        return ProcessParamFile(path, NULL);
    }

    PARAM_LOGV("DacGetParamSecurityLabel %s ", path);
    DIR *pDir = opendir(path);
    PARAM_CHECK(pDir != NULL, return -1, "Read dir :%s failed.%d", path, errno);
    char *fileName = malloc(MAX_BUF_SIZE);
    PARAM_CHECK(fileName != NULL, closedir(pDir);
        return -1, "Failed to malloc for %s", path);

    struct dirent *dp;
    uint32_t count = 0;
    while ((dp = readdir(pDir)) != NULL) {
        if (dp->d_type == DT_DIR) {
            continue;
        }
        char *tmp = strstr(dp->d_name, ".para.dac");
        if (tmp == NULL) {
            continue;
        }
        if (strcmp(tmp, ".para.dac") != 0) {
            continue;
        }
        int ret = PARAM_SPRINTF(fileName, MAX_BUF_SIZE, "%s/%s", path, dp->d_name);
        if (ret <= 0) {
            PARAM_LOGE("Failed to get file name for %s", dp->d_name);
            continue;
        }
        if ((stat(fileName, &st) == 0) && !S_ISDIR(st.st_mode)) {
            count++;
            ret = ProcessParamFile(fileName, NULL);
            if (ret != 0) {
                free(fileName);
                closedir(pDir);
                return ret;
            };
        }
    }
    PARAM_LOGV("Get parameter security label dac number is %d, from %s.", count, path);
    free(fileName);
    closedir(pDir);
    return 0;
}

static int CheckFilePermission(const ParamSecurityLabel *localLabel, const char *fileName, int flags)
{
    UNUSED(flags);
    PARAM_CHECK(localLabel != NULL && fileName != NULL, return -1, "Invalid param");
    return 0;
}

INIT_LOCAL_API int RegisterSecurityDacOps(ParamSecurityOps *ops, int isInit)
{
    PARAM_CHECK(ops != NULL, return -1, "Invalid param");
    PARAM_LOGV("RegisterSecurityDacOps %d", isInit);
    int ret = PARAM_STRCPY(ops->name, sizeof(ops->name), "dac");
    ops->securityInitLabel = InitLocalSecurityLabel;
    ops->securityCheckFilePermission = CheckFilePermission;
#ifdef STARTUP_INIT_TEST
    ops->securityCheckParamPermission = DacCheckParamPermission;
#endif
    ops->securityFreeLabel = FreeLocalSecurityLabel;
    if (isInit) {
        ops->securityGetLabel = DacGetParamSecurityLabel;
    }
    return ret;
}
