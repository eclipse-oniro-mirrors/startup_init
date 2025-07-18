/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#include "init_service_manager.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "cJSON.h"
#include "init.h"
#include "init_group_manager.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_service_file.h"
#include "init_service_socket.h"
#include "init_utils.h"
#include "securec.h"
#include "service_control.h"
#ifdef ASAN_DETECTOR
#include "init_param.h"
#endif

#if defined(ENABLE_HOOK_MGR)
#include "hookmgr.h"
#include "bootstage.h"
#endif

#define VALUE_ATTR_CONSOLE 1
#define VALUE_ATTR_KMSG 2
#define OHOS_KERNEL_PERM_COUNT_KEY "ohos.encaps.count"
#define KERNEL_PERM_PRE "encaps"
#define KERNEL_PERM_MAX_COUNT 64

// All service processes that init will fork+exec.
ServiceSpace g_serviceSpace = { 0 };
static const int CRITICAL_DEFAULT_CRASH_TIME = 240;
// maximum number of crashes within time CRITICAL_DEFAULT_CRASH_TIME for one service
static const int CRITICAL_DEFAULT_CRASH_COUNT =  4;
static const int CRITICAL_CONFIG_ARRAY_LEN = 3;
static const int REBOOT_WAIT_TIME = 100000;
static const int INTERVAL_PER_WAIT = 10;

static void FreeServiceArg(ServiceArgs *arg)
{
    if (arg == NULL) {
        return;
    }
    for (int i = 0; i < arg->count; ++i) {
        if (arg->argv[i] != NULL) {
            free(arg->argv[i]);
            arg->argv[i] = NULL;
        }
    }
    free(arg->argv);
    arg->argv = NULL;
    arg->count = 0;
}

static void FreeServiceSocket(ServiceSocket *sockopt)
{
    // remove service socket list head node from OnDemand socket list before free OnDemand service socket
    RemoveOnDemandSocket(sockopt);
    while (sockopt != NULL) {
        ServiceSocket *tmp = sockopt;
        if (tmp->sockFd >= 0) {
            close(tmp->sockFd);
            tmp->sockFd = -1;
        }
        sockopt = sockopt->next;
        free(tmp);
    }
    sockopt = NULL;
    return;
}

Service *AddService(const char *name)
{
    // check service in group
    if (CheckNodeValid(NODE_TYPE_SERVICES, name) != 0) {
        INIT_LOGI("Service %s not exist in group ", name);
        return NULL;
    }
    InitGroupNode *node = AddGroupNode(NODE_TYPE_SERVICES, name);
    if (node == NULL) {
        INIT_LOGE("Failed to create service name %s", name);
        return NULL;
    }
    if (node->data.service != NULL) {
        ReleaseService(node->data.service);
        node->data.service = NULL;
    }
    Service *service = (Service *)calloc(1, sizeof(Service));
    INIT_ERROR_CHECK(service != NULL, return NULL, "Failed to malloc for service");
    node->data.service = service;
    service->name = node->name;
    service->status = SERVICE_IDLE;
    service->cpuSet = NULL;
    service->pid = -1;
    service->context.type = INIT_CONTEXT_MAIN;
    service->lastErrno = INIT_OK;
    OH_ListInit(&service->extDataNode);
    g_serviceSpace.serviceCount++;
    INIT_LOGV("AddService %s", node->name);
    return service;
}

static void FreeServiceFile(ServiceFile *fileOpt)
{
    while (fileOpt != NULL) {
        ServiceFile *tmp = fileOpt;
        if (tmp->fd >= 0) {
            close(tmp->fd);
            tmp->fd = -1;
        }
        fileOpt = fileOpt->next;
        free(tmp);
    }
    fileOpt = NULL;
    return;
}

static void ExecuteServiceClear(Service *service)
{
#if defined(ENABLE_HOOK_MGR)
    // clear ext data
    SERVICE_INFO_CTX ctx = {0};
    ctx.serviceName = service->name;
    HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_CLEAR, (void *)&ctx, NULL);
#endif
}

static void FreeServiceKernelPerm(Service *service)
{
    if (service->kernelPerms != NULL) {
        free(service->kernelPerms);
        service->kernelPerms = NULL;
    }
}

void ReleaseService(Service *service)
{
    INIT_CHECK(service != NULL, return);
    FreeServiceArg(&service->pathArgs);
    FreeServiceArg(&service->writePidArgs);
    FreeServiceArg(&service->capsArgs);
    FreeServiceArg(&service->permArgs);
    FreeServiceArg(&service->permAclsArgs);
    FreeServiceKernelPerm(service);
    if (service->servPerm.caps != NULL) {
        free(service->servPerm.caps);
        service->servPerm.caps = NULL;
    }
    service->servPerm.capsCnt = 0;
    if (service->servPerm.gIDArray != NULL) {
        free(service->servPerm.gIDArray);
        service->servPerm.gIDArray = NULL;
    }
    service->servPerm.gIDCnt = 0;

    FreeServiceSocket(service->socketCfg);
    FreeServiceFile(service->fileCfg);

    if (service->apl != NULL) {
        free(service->apl);
        service->apl = NULL;
    }

    if (service->env != NULL) {
        free(service->env);
        service->env = NULL;
    }

    for (size_t i = 0; i < JOB_ON_MAX; i++) {
        if (service->serviceJobs.jobsName[i] != NULL) {
            free(service->serviceJobs.jobsName[i]);
        }
        service->serviceJobs.jobsName[i] = NULL;
    }
    if (service->cpuSet != NULL) {
        free(service->cpuSet);
        service->cpuSet = NULL;
    }
    if (service->restartArg != NULL) {
        free(service->restartArg);
        service->restartArg = NULL;
    }
    CloseServiceFds(service, true);
    ExecuteServiceClear(service);
    g_serviceSpace.serviceCount--;
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_SERVICES, service->name);
    INIT_CHECK(groupNode == NULL, groupNode->data.service = NULL);
    free(service);
}

static char *GetStringValue(const cJSON *json, const char *name, size_t *strLen)
{
    char *fieldStr = cJSON_GetStringValue(cJSON_GetObjectItem(json, name));
    INIT_CHECK(fieldStr != NULL, return NULL);
    *strLen = strlen(fieldStr);
    return fieldStr;
}

cJSON *GetArrayItem(const cJSON *fileRoot, int *arrSize, const char *arrName)
{
    cJSON *arrItem = cJSON_GetObjectItemCaseSensitive(fileRoot, arrName);
    if (!cJSON_IsArray(arrItem)) {
        return NULL;
    }
    *arrSize = cJSON_GetArraySize(arrItem);
    INIT_CHECK_RETURN_VALUE(*arrSize > 0, NULL);
    return arrItem;
}

static int GetServiceArgs(const cJSON *argJson, const char *name, int maxCount, ServiceArgs *args)
{
    INIT_ERROR_CHECK(argJson != NULL, return SERVICE_FAILURE, "Invalid argJson");
    cJSON *obj = cJSON_GetObjectItem(argJson, name);
    INIT_CHECK(obj != NULL, return SERVICE_FAILURE);

    int ret = cJSON_IsArray(obj);
    INIT_ERROR_CHECK(ret, return SERVICE_FAILURE, "Invalid type");
    int count = cJSON_GetArraySize(obj);
    INIT_ERROR_CHECK((count > 0) && (count < maxCount), return SERVICE_FAILURE,
        "Args size is wrong %s, %d", name, count);
    if ((args->argv != NULL) && (args->count > 0)) {
        FreeServiceArg(args);
    }
    args->argv = (char **)malloc((count + 1) * sizeof(char *));
    INIT_ERROR_CHECK(args->argv != NULL, return SERVICE_FAILURE, "Failed to malloc for argv");
    for (int i = 0; i < count + 1; ++i) {
        args->argv[i] = NULL;
    }
    // ServiceArgs have a variety of uses, some requiring a NULL ending, some not
    if (strcmp(name, D_CAPS_STR_IN_CFG) != 0 && strcmp(name, "permission_acls") != 0 &&
        strcmp(name, "permission") != 0) {
        args->count = count + 1;
    } else {
        args->count = count;
    }
    for (int i = 0; i < count; ++i) {
        char *curParam = cJSON_GetStringValue(cJSON_GetArrayItem(obj, i));
        INIT_ERROR_CHECK(curParam != NULL, return SERVICE_FAILURE, "Invalid arg %d", i);
        INIT_ERROR_CHECK(strlen(curParam) <= MAX_ONE_ARG_LEN, return SERVICE_FAILURE, "Arg %s is tool long", curParam);
        args->argv[i] = strdup(curParam);
        INIT_ERROR_CHECK(args->argv[i] != NULL, return SERVICE_FAILURE, "Failed to duplicate argument %s", curParam);
    }
    return SERVICE_SUCCESS;
}

static int GetUid(cJSON *json, uid_t *uid)
{
    INIT_CHECK_RETURN_VALUE(json != NULL, SERVICE_SUCCESS);
    if (cJSON_IsString(json)) {
        char *str = cJSON_GetStringValue(json);
        INIT_ERROR_CHECK(str != NULL, return SERVICE_FAILURE, "Invalid str");
        *uid = DecodeUid(str);
    } else if (cJSON_IsNumber(json)) {
        *uid = (uid_t)cJSON_GetNumberValue(json);
    } else {
        *uid = (uid_t)(-1);
    }
    INIT_CHECK_RETURN_VALUE(*uid != (uid_t)(-1), SERVICE_FAILURE);
    return SERVICE_SUCCESS;
}

static int GetGid(cJSON *json, gid_t *gid, Service *curServ)
{
    INIT_CHECK_RETURN_VALUE(json != NULL, SERVICE_SUCCESS);
    if (cJSON_IsString(json)) {
        char *str = cJSON_GetStringValue(json);
        INIT_ERROR_CHECK(str != NULL, return SERVICE_FAILURE, "Failed to get gid for %s", curServ->name);
        *gid = DecodeGid(str);
    } else if (cJSON_IsNumber(json)) {
        *gid = (gid_t)cJSON_GetNumberValue(json);
    } else {
        INIT_LOGW("Service %s with invalid gid configuration", curServ->name);
        *gid = -1; // Invalid gid, set as -1
    }
    INIT_CHECK_RETURN_VALUE(*gid != (gid_t)(-1), SERVICE_FAILURE);
    return SERVICE_SUCCESS;
}

static int GetServiceGids(const cJSON *curArrItem, Service *curServ)
{
    int gidCount;
    cJSON *arrItem = cJSON_GetObjectItemCaseSensitive(curArrItem, GID_STR_IN_CFG);
    if (!arrItem) {
        return SERVICE_SUCCESS;
    } else if (!cJSON_IsArray(arrItem)) {
        gidCount = 1;
    } else {
        gidCount = cJSON_GetArraySize(arrItem);
    }
    INIT_ERROR_CHECK((gidCount != 0) && (gidCount <= NGROUPS_MAX + 1), return SERVICE_FAILURE,
        "Invalid gid count %d", gidCount);
    if (curServ->servPerm.gIDArray != NULL) {
        free(curServ->servPerm.gIDArray);
    }
    curServ->servPerm.gIDArray = (gid_t *)malloc(sizeof(gid_t) * (gidCount + 1));
    INIT_ERROR_CHECK(curServ->servPerm.gIDArray != NULL, return SERVICE_FAILURE, "Failed to malloc err=%d", errno);
    curServ->servPerm.gIDCnt = gidCount;

    gid_t gid;
    if (!cJSON_IsArray(arrItem)) {
        int ret = GetGid(arrItem, &gid, curServ);
        INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE,
            "Service error %s, failed to get gid.", curServ->name);
        curServ->servPerm.gIDArray[0] = gid;
        return SERVICE_SUCCESS;
    }
    int gidArrayIndex = 0;
    for (int i = 0; i < gidCount; ++i) {
        cJSON *item = cJSON_GetArrayItem(arrItem, i);
        int ret = GetGid(item, &gid, curServ);
        if (ret != 0) {
            INIT_LOGW("Service warning %s, failed to get gid.", curServ->name);
            continue;
        }
        curServ->servPerm.gIDArray[gidArrayIndex++] = gid;
    }
    if (gidArrayIndex == 0) {
        curServ->servPerm.gIDArray[gidArrayIndex++] = curServ->servPerm.uID;
    }
    curServ->servPerm.gIDCnt = gidArrayIndex;
    return SERVICE_SUCCESS;
}

static int GetServiceAttr(const cJSON *curArrItem, Service *curServ, const char *attrName, int flag,
    int (*processAttr)(Service *curServ, const char *attrName, int value, int flag))
{
    cJSON *filedJ = cJSON_GetObjectItem(curArrItem, attrName);
    if (filedJ == NULL) {
        return SERVICE_SUCCESS;
    }
    INIT_ERROR_CHECK(cJSON_IsNumber(filedJ), return SERVICE_FAILURE,
        "Service error %s is null or is not a number %s", curServ->name, attrName);
    curServ->attribute &= ~flag;
    int value = (int)cJSON_GetNumberValue(filedJ);
    if (processAttr == NULL) {
        if (value == 1) {
            curServ->attribute |= flag;
        }
        return 0;
    }
    return processAttr(curServ, attrName, value, flag);
}

static int ParseSocketFamily(cJSON *json, ServiceSocket *sockopt)
{
    sockopt->family = AF_UNIX;
    size_t strLen = 0;
    char *stringValue = GetStringValue(json, "family", &strLen);
    INIT_ERROR_CHECK((stringValue != NULL) && (strLen > 0), return SERVICE_FAILURE,
        "Failed to get string for family");
    if (strcmp(stringValue, "AF_UNIX") == 0) {
        sockopt->family = AF_UNIX;
    } else if (strncmp(stringValue, "AF_NETLINK", strLen) == 0) {
        sockopt->family = AF_NETLINK;
    }
    return 0;
}

static int ParseSocketType(cJSON *json, ServiceSocket *sockopt)
{
    sockopt->type = SOCK_SEQPACKET;
    size_t strLen = 0;
    char *stringValue = GetStringValue(json, "type", &strLen);
    INIT_ERROR_CHECK((stringValue != NULL) && (strLen > 0), return SERVICE_FAILURE,
        "Failed to get string for type");
    if (strncmp(stringValue, "SOCK_SEQPACKET", strLen) == 0) {
        sockopt->type = SOCK_SEQPACKET;
    } else if (strncmp(stringValue, "SOCK_STREAM", strLen) == 0) {
        sockopt->type = SOCK_STREAM;
    } else if (strncmp(stringValue, "SOCK_DGRAM", strLen) == 0) {
        sockopt->type = SOCK_DGRAM;
    }
    return 0;
}

static int ParseSocketProtocol(cJSON *json, ServiceSocket *sockopt)
{
    sockopt->protocol = 0;
    size_t strLen = 0;
    char *stringValue = GetStringValue(json, "protocol", &strLen);
    INIT_ERROR_CHECK((stringValue != NULL) && (strLen > 0), return SERVICE_FAILURE,
        "Failed to get string for protocol");
    if (strncmp(stringValue, "default", strLen) == 0) {
        sockopt->protocol = 0;
    } else if (strncmp(stringValue, "NETLINK_KOBJECT_UEVENT", strLen) == 0) {
#ifndef __LITEOS_A__
        sockopt->protocol = NETLINK_KOBJECT_UEVENT;
#else
        return -1;
#endif
    }
    return 0;
}

static int ParseSocketOption(cJSON *json, ServiceSocket *sockopt)
{
    sockopt->option = 0;
    unsigned int tempType = 0;
    int typeCnt = 0;
    char *stringValue = NULL;
    cJSON *typeItem = NULL;
    cJSON *typeArray = GetArrayItem(json, &typeCnt, "option");
    INIT_CHECK((typeArray != NULL) && (typeCnt > 0), return 0);
    for (int i = 0; i < typeCnt; ++i) {
        typeItem = cJSON_GetArrayItem(typeArray, i);
        INIT_CHECK_RETURN_VALUE(cJSON_IsString(typeItem), SERVICE_FAILURE);
        stringValue = cJSON_GetStringValue(typeItem);
        INIT_ERROR_CHECK(stringValue != NULL, return SERVICE_FAILURE, "Failed to get string for type");
        if (strncmp(stringValue, "SOCKET_OPTION_PASSCRED", strlen(stringValue)) == 0) {
            sockopt->option |= SOCKET_OPTION_PASSCRED;
        } else if (strncmp(stringValue, "SOCKET_OPTION_RCVBUFFORCE", strlen(stringValue)) == 0) {
            sockopt->option |= SOCKET_OPTION_RCVBUFFORCE;
        } else if (strncmp(stringValue, "SOCK_CLOEXEC", strlen(stringValue)) == 0) {
            tempType |= SOCK_CLOEXEC;
        } else if (strncmp(stringValue, "SOCK_NONBLOCK", strlen(stringValue)) == 0) {
            tempType |= SOCK_NONBLOCK;
        }
    }
    if (tempType != 0) {
        sockopt->type |= tempType;
    }
    return 0;
}

static int AddServiceSocket(cJSON *json, Service *service)
{
    size_t strLen = 0;
    char* fieldStr = GetStringValue(json, "name", &strLen);
    INIT_ERROR_CHECK(fieldStr != NULL, return SERVICE_FAILURE, "Failed to get socket name");
    INIT_ERROR_CHECK(strLen <= MAX_SERVICE_NAME, return SERVICE_FAILURE, "socket name exceeds length limit");
    ServiceSocket *sockopt = (ServiceSocket *)calloc(1, sizeof(ServiceSocket) + strLen + 1);
    INIT_INFO_CHECK(sockopt != NULL, return SERVICE_FAILURE, "Failed to malloc for service %s", service->name);

    int ret = strcpy_s(sockopt->name, strLen + 1, fieldStr);
    INIT_INFO_CHECK(ret == 0, free(sockopt); sockopt = NULL; return SERVICE_FAILURE,
        "Failed to copy socket name %s", fieldStr);
    sockopt->sockFd = -1;
    sockopt->watcher = NULL;
    sockopt->service = service;

    ret = ParseSocketFamily(json, sockopt);
    INIT_ERROR_CHECK(ret == 0, free(sockopt);
        return SERVICE_FAILURE, "Failed to parse socket family");
    ret = ParseSocketType(json, sockopt);
    INIT_ERROR_CHECK(ret == 0, free(sockopt);
        return SERVICE_FAILURE, "Failed to parse socket type");
    ret = ParseSocketProtocol(json, sockopt);
    INIT_ERROR_CHECK(ret == 0, free(sockopt);
        return SERVICE_FAILURE, "Failed to parse socket protocol");

    char *stringValue = GetStringValue(json, "permissions", &strLen);
    INIT_ERROR_CHECK((stringValue != NULL) && (strLen > 0), free(sockopt);
        return SERVICE_FAILURE, "Failed to get string for permissions");
    sockopt->perm = strtoul(stringValue, 0, OCTAL_BASE);
    stringValue = GetStringValue(json, "uid", &strLen);
    INIT_ERROR_CHECK((stringValue != NULL) && (strLen > 0), free(sockopt);
        return SERVICE_FAILURE, "Failed to get string for uid");
    sockopt->uid = DecodeUid(stringValue);
    stringValue = GetStringValue(json, "gid", &strLen);
    INIT_ERROR_CHECK((stringValue != NULL) && (strLen > 0), free(sockopt);
        return SERVICE_FAILURE, "Failed to get string for gid");
    sockopt->gid = DecodeGid(stringValue);
    INIT_ERROR_CHECK((sockopt->uid != (uid_t)-1) && (sockopt->gid != (uid_t)-1), free(sockopt);
        return SERVICE_FAILURE, "Invalid uid or gid");
    ret = ParseSocketOption(json, sockopt);
    INIT_ERROR_CHECK(ret == 0, free(sockopt);
        return SERVICE_FAILURE, "Failed to parse socket option");

    sockopt->next = NULL;
    if (service->socketCfg == NULL) {
        service->socketCfg = sockopt;
    } else {
        sockopt->next = service->socketCfg->next;
        service->socketCfg->next = sockopt;
    }
    return SERVICE_SUCCESS;
}

static int ParseServiceSocket(const cJSON *curArrItem, Service *curServ)
{
    int sockCnt = 0;
    cJSON *filedJ = GetArrayItem(curArrItem, &sockCnt, "socket");
    INIT_CHECK(filedJ != NULL && sockCnt > 0, return SERVICE_FAILURE);

    CloseServiceSocket(curServ);
    FreeServiceSocket(curServ->socketCfg);
    int ret = 0;
    curServ->socketCfg = NULL;
    for (int i = 0; i < sockCnt; ++i) {
        cJSON *sockJ = cJSON_GetArrayItem(filedJ, i);
        ret = AddServiceSocket(sockJ, curServ);
        if (ret != 0) {
            return ret;
        }
    }
    if (IsOnDemandService(curServ)) {
        ret = CreateSocketForService(curServ);
    }
    return ret;
}

static int AddServiceFile(cJSON *json, Service *service)
{
    if (!cJSON_IsString(json) || !cJSON_GetStringValue(json)) {
        return SERVICE_FAILURE;
    }
    char *fileStr = cJSON_GetStringValue(json);
    char *opt[FILE_OPT_NUMS] = {
        NULL,
    };
    int num = SplitString(fileStr, " ", opt, FILE_OPT_NUMS);
    INIT_CHECK_RETURN_VALUE(num == FILE_OPT_NUMS, SERVICE_FAILURE);
    if (opt[SERVICE_FILE_NAME] == NULL || opt[SERVICE_FILE_FLAGS] == NULL || opt[SERVICE_FILE_PERM] == NULL ||
        opt[SERVICE_FILE_UID] == NULL || opt[SERVICE_FILE_GID] == NULL) {
        INIT_LOGE("Invalid file opt");
        return SERVICE_FAILURE;
    }
    ServiceFile *fileOpt = (ServiceFile *)calloc(1, sizeof(ServiceFile) + strlen(opt[SERVICE_FILE_NAME]) + 1);
    INIT_INFO_CHECK(fileOpt != NULL, return SERVICE_FAILURE, "Failed to calloc for file %s", opt[SERVICE_FILE_NAME]);
    int ret = strcpy_s(fileOpt->fileName, strlen(opt[SERVICE_FILE_NAME]) + 1, opt[SERVICE_FILE_NAME]);
    INIT_INFO_CHECK(ret == 0, free(fileOpt);
        return SERVICE_FAILURE, "Failed to copy file name %s", opt[SERVICE_FILE_NAME]);
    if (strcmp(opt[SERVICE_FILE_FLAGS], "rd") == 0) {
        fileOpt->flags = O_RDONLY;
    } else if (strcmp(opt[SERVICE_FILE_FLAGS], "wd") == 0) {
        fileOpt->flags = O_WRONLY;
    } else if (strcmp(opt[SERVICE_FILE_FLAGS], "rw") == 0) {
        fileOpt->flags = O_RDWR;
    } else {
        INIT_LOGE("Failed file flags %s", opt[SERVICE_FILE_FLAGS]);
        free(fileOpt);
        fileOpt = NULL;
        return SERVICE_FAILURE;
    }
    fileOpt->perm = strtoul(opt[SERVICE_FILE_PERM], 0, OCTAL_BASE);
    fileOpt->uid = DecodeUid(opt[SERVICE_FILE_UID]);
    fileOpt->gid = DecodeGid(opt[SERVICE_FILE_GID]);
    if (fileOpt->uid == (uid_t)-1 || fileOpt->gid == (gid_t)-1) {
        free(fileOpt);
        fileOpt = NULL;
        INIT_LOGE("Invalid uid or gid");
        return SERVICE_FAILURE;
    }
    fileOpt->fd = -1;
    fileOpt->next = NULL;
    if (service->fileCfg == NULL) {
        service->fileCfg = fileOpt;
    } else {
        fileOpt->next = service->fileCfg->next;
        service->fileCfg->next = fileOpt;
    }
    return SERVICE_SUCCESS;
}

static int ParseServiceFile(const cJSON *curArrItem, Service *curServ)
{
    int fileCnt = 0;
    cJSON *filedJ = GetArrayItem(curArrItem, &fileCnt, "file");
    INIT_CHECK(filedJ != NULL && fileCnt > 0, return SERVICE_FAILURE);
    FreeServiceFile(curServ->fileCfg);
    int ret = 0;
    curServ->fileCfg = NULL;
    for (int i = 0; i < fileCnt; ++i) {
        cJSON *fileJ = cJSON_GetArrayItem(filedJ, i);
        ret = AddServiceFile(fileJ, curServ);
        if (ret != 0) {
            break;
        }
    }
    return ret;
}

static int GetServiceOnDemand(const cJSON *curArrItem, Service *curServ)
{
    cJSON *item = cJSON_GetObjectItem(curArrItem, "ondemand");
    if (item == NULL) {
        return SERVICE_SUCCESS;
    }

    INIT_ERROR_CHECK(cJSON_IsBool(item), return SERVICE_FAILURE,
        "Service : %s ondemand value only support bool.", curServ->name);
    curServ->attribute &= ~SERVICE_ATTR_ONDEMAND;

    INIT_INFO_CHECK(cJSON_IsTrue(item), return SERVICE_SUCCESS,
        "Service : %s ondemand value is false, it should be pulled up by init", curServ->name);
    if (curServ->attribute & SERVICE_ATTR_CRITICAL) {
        INIT_LOGE("Service : %s is invalid which has both critical and ondemand attribute", curServ->name);
        return SERVICE_FAILURE;
    }

    curServ->attribute |= SERVICE_ATTR_ONDEMAND;
    return SERVICE_SUCCESS;
}

static int GetServiceSetuid(const cJSON *curArrItem, Service *curServ)
{
    cJSON *item = cJSON_GetObjectItem(curArrItem, "setuid");
    if (item == NULL) {
        return SERVICE_SUCCESS;
    }

    INIT_ERROR_CHECK(cJSON_IsBool(item), return SERVICE_FAILURE,
        "Service : %s setuid value only support bool.", curServ->name);
    curServ->attribute &= ~SERVICE_ATTR_SETUID;

    INIT_INFO_CHECK(cJSON_IsTrue(item), return SERVICE_SUCCESS,
        "Service : %s setuid value is false, it should be pulled up by init", curServ->name);

    curServ->attribute |= SERVICE_ATTR_SETUID;
    return SERVICE_SUCCESS;
}

static int GetMapValue(const char *name, const InitArgInfo *infos, int argNum, int defValue)
{
    if ((argNum == 0) || (infos == NULL) || (name == NULL)) {
        return defValue;
    }
    for (int i = 0; i < argNum; i++) {
        if (strcmp(infos[i].name, name) == 0) {
            return infos[i].value;
        }
    }
    return defValue;
}

static int GetServiceMode(Service *service, const cJSON *json)
{
    const InitArgInfo startModeMap[] = {
        {"condition", START_MODE_CONDITION},
        {"boot", START_MODE_BOOT},
        {"normal", START_MODE_NORMAL}
    };
    const InitArgInfo endModeMap[] = {
        {"pre-fork", END_PRE_FORK},
        {"after-fork", END_AFTER_FORK},
        {"after-exec", END_AFTER_EXEC},
        {"ready", END_RECV_READY}
    };
    service->startMode = START_MODE_NORMAL;
    service->endMode = END_AFTER_EXEC;
    char *value = cJSON_GetStringValue(cJSON_GetObjectItem(json, "start-mode"));
    if (value != NULL) {
        service->startMode = GetMapValue(value,
            (InitArgInfo *)startModeMap, (int)ARRAY_LENGTH(startModeMap), START_MODE_NORMAL);
    }
    value = cJSON_GetStringValue(cJSON_GetObjectItem(json, "end-mode"));
    if (value != NULL) {
        service->endMode = GetMapValue(value,
            (InitArgInfo *)endModeMap, (int)ARRAY_LENGTH(endModeMap), END_AFTER_EXEC);
    }
    return 0;
}

static int GetServiceJobs(Service *service, cJSON *json)
{
    const char *jobTypes[] = {
        "on-boot", "pre-start", "on-start", "on-stop", "on-restart"
    };
    for (int i = 0; i < (int)ARRAY_LENGTH(jobTypes); i++) {
        char *jobName = cJSON_GetStringValue(cJSON_GetObjectItem(json, jobTypes[i]));
        if (jobName != NULL) {
            if (service->serviceJobs.jobsName[i] != NULL) {
                DelGroupNode(NODE_TYPE_JOBS, service->serviceJobs.jobsName[i]);
                free(service->serviceJobs.jobsName[i]);
            }
            service->serviceJobs.jobsName[i] = strdup(jobName);
            if (service->serviceJobs.jobsName[i] == NULL) {
                return SERVICE_FAILURE;
            }
            // save job name for group job check
            AddGroupNode(NODE_TYPE_JOBS, jobName);
        }
    }
    return SERVICE_SUCCESS;
}

int  GetCritical(const cJSON *curArrItem, Service *curServ, const char *attrName, int flag)
{
    int criticalSize = 0;
    curServ->crashCount = CRITICAL_DEFAULT_CRASH_COUNT;
    curServ->crashTime = CRITICAL_DEFAULT_CRASH_TIME;
    cJSON *arrItem = cJSON_GetObjectItem(curArrItem, attrName);
    if (arrItem == NULL) {
        return SERVICE_SUCCESS;
    }

    if (cJSON_IsNumber(arrItem)) {
        return GetServiceAttr(curArrItem, curServ, attrName, flag, NULL);
    } else if (cJSON_IsArray(arrItem)) {
        criticalSize = cJSON_GetArraySize(arrItem);
        cJSON *attrItem = cJSON_GetArrayItem(arrItem, 0); // 0 : critical attribute index
        if (attrItem == NULL || !cJSON_IsNumber(attrItem)) {
            INIT_LOGE("%s critical invalid", curServ->name);
            return SERVICE_FAILURE;
        }
        int attrValue = (int)cJSON_GetNumberValue(attrItem);
        curServ->attribute &= ~flag;
        if (criticalSize == 1) {
            if (attrValue == 1) {
                curServ->attribute |= flag;
            }
        } else if (criticalSize == CRITICAL_CONFIG_ARRAY_LEN) {
            cJSON *crashCountItem = cJSON_GetArrayItem(arrItem, 1); // 1 : critical crash count index
            INIT_ERROR_CHECK(crashCountItem != NULL, return SERVICE_FAILURE, "%s critical invalid", curServ->name);
            int value = (int)cJSON_GetNumberValue(crashCountItem);
            INIT_ERROR_CHECK(value > 0, return SERVICE_FAILURE, "%s critical crashc ount invalid", curServ->name);
            curServ->crashCount = value;

            cJSON *crashTimeItem = cJSON_GetArrayItem(arrItem, 2); // 2 : critical crash time index
            INIT_ERROR_CHECK(crashTimeItem != NULL, return SERVICE_FAILURE, "%s critical invalid", curServ->name);
            value = (int)cJSON_GetNumberValue(crashTimeItem);
            INIT_ERROR_CHECK(value > 0, return SERVICE_FAILURE, "%s critical crash time invalid", curServ->name);
            curServ->crashTime = value;

            if (attrValue == 1) {
                curServ->attribute |= flag;
            }
        } else {
            curServ->attribute &= ~flag;
            INIT_LOGE("%s critical param invalid", curServ->name);
            return SERVICE_FAILURE;
        }
    } else {
        INIT_LOGE("%s critical type error", curServ->name);
        return SERVICE_FAILURE;
    }
    return SERVICE_SUCCESS;
}

static int GetCpuArgs(const cJSON *argJson, const char *name, Service *service)
{
    INIT_ERROR_CHECK(argJson != NULL, return SERVICE_FAILURE, "Invalid argJson");
    cJSON *obj = cJSON_GetObjectItem(argJson, name);
    INIT_CHECK(obj != NULL, return SERVICE_FAILURE);

    int ret = cJSON_IsArray(obj);
    INIT_ERROR_CHECK(ret, return SERVICE_FAILURE, "Invalid type");
    int count = cJSON_GetArraySize(obj);
    int cpus = -1;
    int cpuNumMax = sysconf(_SC_NPROCESSORS_CONF);
    if (count > 0 && service->cpuSet == NULL) {
        service->cpuSet = malloc(sizeof(cpu_set_t));
        INIT_ERROR_CHECK(service->cpuSet != NULL, return SERVICE_FAILURE, "Failed to malloc for cpuset");
    }
    CPU_ZERO(service->cpuSet);
    for (int i = 0; i < count; ++i) {
        cJSON *item = cJSON_GetArrayItem(obj, i);
        INIT_ERROR_CHECK(item != NULL, return SERVICE_FAILURE, "prase invalid");
        cpus = (int)cJSON_GetNumberValue(item);
        if (cpuNumMax <= cpus) {
            INIT_LOGW("%s core number %d of CPU cores does not exist", service->name, cpus);
            continue;
        }
        if (CPU_ISSET(cpus, service->cpuSet)) {
            continue;
        }
        CPU_SET(cpus, service->cpuSet);
    }
    return SERVICE_SUCCESS;
}

static int GetServiceSandbox(const cJSON *curItem, Service *service)
{
    cJSON *item = cJSON_GetObjectItem(curItem, "sandbox");
    if (item == NULL) {
        return SERVICE_SUCCESS;
    }

    INIT_ERROR_CHECK(cJSON_IsNumber(item), return SERVICE_FAILURE,
        "Service : %s sandbox value only support number.", service->name);
    int isSandbox = (int)cJSON_GetNumberValue(item);
    if (isSandbox == 0) {
        MarkServiceWithoutSandbox(service);
    } else {
        MarkServiceWithSandbox(service);
    }

    return SERVICE_SUCCESS;
}

#ifdef ASAN_DETECTOR
static int WrapPath(char *dest, size_t len, char *source, int i)
{
    char *q = source;
    char *p = dest;
    if (q == NULL) {
        return -1;
    }

    while (*p != '\0') {
        if (--i <= 0) {
            int ret = memmove_s(p + strlen(source), len, p, strlen(p) + 1);
            INIT_ERROR_CHECK(ret == 0, return -1, "Dest is %s, source is %s, ret is %d.", dest, source, ret);
            break;
        }
        p++;
    }
    while (*q != '\0') {
        *p = *q;
        p++;
        q++;
    }

    return 0;
}

static int GetWrapServiceNameValue(const char *serviceName)
{
    char wrapServiceNameKey[PARAM_VALUE_LEN_MAX] = {0};
    char wrapServiceNameValue[PARAM_VALUE_LEN_MAX] = {0};
    unsigned int valueLen = PARAM_VALUE_LEN_MAX;

    int len = sprintf_s(wrapServiceNameKey, PARAM_VALUE_LEN_MAX, "wrap.%s", serviceName);
    INIT_INFO_CHECK(len > 0 && (len < PARAM_VALUE_LEN_MAX), return -1, "Invalid to format wrapServiceNameKey");

    int ret = SystemReadParam(wrapServiceNameKey, wrapServiceNameValue, &valueLen);
    INIT_INFO_CHECK(ret == 0, return -1, "Not wrap %s.", serviceName);
    INIT_LOGI("Asan: GetParameter %s the value is %s.", serviceName, wrapServiceNameValue);
    return 0;
}

void SetServicePathWithAsan(Service *service)
{
    char tmpPathName[MAX_ONE_ARG_LEN] = {0};
    int anchorPos = -1;

    if (GetWrapServiceNameValue(service->name) != 0) {
        return;
    }

    int ret = strcpy_s(tmpPathName, MAX_ONE_ARG_LEN, service->pathArgs.argv[0]);
    INIT_ERROR_CHECK(ret == 0, return, "Asan: failed to copy service path %s", service->pathArgs.argv[0]);

    if (strstr(tmpPathName, "/system/bin") != NULL) {
        anchorPos = strlen("/system") + 1;
    } else if (strstr(tmpPathName, "/vendor/bin") != NULL) {
        anchorPos = strlen("/vendor") + 1;
    } else {
        INIT_LOGE("Asan: %s is not a system or vendor binary", tmpPathName);
        return;
    }

    ret = WrapPath(tmpPathName, MAX_ONE_ARG_LEN, "/asan", anchorPos);
    INIT_ERROR_CHECK(ret == 0, return, "Asan: failed to add asan path.");
    free(service->pathArgs.argv[0]);
    service->pathArgs.argv[0] = strdup(tmpPathName);
    INIT_ERROR_CHECK(service->pathArgs.argv[0] != NULL, return, "Asan: failed dup path.");
    INIT_LOGI("Asan: replace module %s with %s successfully.", service->name, service->pathArgs.argv[0]);

    return;
}
#endif

static bool AddPermissionItemToKernelPerm(cJSON *kernelPerm, cJSON *permissionItem)
{
    const char *key = permissionItem->string;
    if (cJSON_IsNumber(permissionItem)) {
        if (!cJSON_AddNumberToObject(kernelPerm, key, permissionItem->valueint)) {
            INIT_LOGE("GetKernelPerm: add int value failed");
            return false;
        }
    } else if (cJSON_IsBool(permissionItem)) {
        if (!cJSON_AddBoolToObject(kernelPerm, key, permissionItem->valueint)) {
            INIT_LOGE("GetKernelPerm: add bool value failed");
            return false;
        }
    } else {
        INIT_LOGE("GetKernelPerm: value type not supported");
        return false;
    }
    return true;
}

static bool MakeKernelPerm(const cJSON *cJsonKernelPerms, int count, cJSON *root, cJSON *kernelPerm)
{
    INIT_ERROR_CHECK(cJSON_AddNumberToObject(kernelPerm, OHOS_KERNEL_PERM_COUNT_KEY, count) != NULL, \
                        return false, "MakeKernelPerm: failed to add kernelPerm count");
    for (int i = 0; i < count; i++) {
        cJSON *permission = cJSON_GetArrayItem(cJsonKernelPerms, i);
        INIT_ERROR_CHECK(permission != NULL, return false, "MakeKernelPerm: failed to get permission");
        INIT_ERROR_CHECK(AddPermissionItemToKernelPerm(kernelPerm, permission), return false, \
                            "MakeKernelPerm: failed to add permission");
    }
    INIT_ERROR_CHECK(cJSON_AddItemToObject(root, KERNEL_PERM_PRE, kernelPerm), return false, \
                        "MakeKernelPerm: failed to add kernelPerm to root");
    return true;
}

static void GetKernelPerm(const cJSON *curItem, Service *service)
{
    service->kernelPerms = NULL;
    cJSON *cJsonKernelPerms = cJSON_GetObjectItemCaseSensitive(curItem, "kernel_permission");
    INIT_CHECK(cJsonKernelPerms != NULL, return);
    INIT_LOGI("GetKernelPerm: build kernelPerm str");
    int count = cJSON_GetArraySize(cJsonKernelPerms);
    INIT_INFO_CHECK(count > 0, return, "GetKernelPerm: count is zero");
    INIT_ERROR_CHECK(count < KERNEL_PERM_MAX_COUNT, return, "GetKernelPerm: too many kernelPerm");
    cJSON *root = cJSON_CreateObject();
    INIT_ERROR_CHECK(root != NULL, return, "GetKernelPerm: failed to create root");
    cJSON *kernelPerm = cJSON_CreateObject();
    if (kernelPerm == NULL) {
        cJSON_Delete(root);
        INIT_LOGE("GetKernelPerm: failed to create kernelPerm");
        return;
    }
    if (MakeKernelPerm(cJsonKernelPerms, count, root, kernelPerm)) {
        service->kernelPerms = cJSON_PrintUnformatted(root);
        if (service->kernelPerms == NULL) {
            INIT_LOGE("GetKernelPerm: failed to get kernelPerm str");
        }
        cJSON_Delete(root);
    } else {
        cJSON_Delete(root);
        cJSON_Delete(kernelPerm);
    }
}

static void ParseOneServiceArgs(const cJSON *curItem, Service *service)
{
    GetServiceArgs(curItem, "writepid", MAX_WRITEPID_FILES, &service->writePidArgs);
    GetServiceArgs(curItem, D_CAPS_STR_IN_CFG, MAX_WRITEPID_FILES, &service->capsArgs);
    GetServiceArgs(curItem, "permission", MAX_WRITEPID_FILES, &service->permArgs);
    GetServiceArgs(curItem, "permission_acls", MAX_WRITEPID_FILES, &service->permAclsArgs);
    GetKernelPerm(curItem, service);
    size_t strLen = 0;
    char *fieldStr = GetStringValue(curItem, APL_STR_IN_CFG, &strLen);
    if (fieldStr != NULL) {
        if (service->apl != NULL) {
            free(service->apl);
        }
        service->apl = strdup(fieldStr);
        INIT_CHECK(service->apl != NULL, return);
    }
    (void)GetCpuArgs(curItem, CPU_CORE_STR_IN_CFG, service);
}

static int SetConsoleValue(Service *service, const char *attrName, int value, int flag)
{
    UNUSED(attrName);
    UNUSED(flag);
    INIT_ERROR_CHECK(service != NULL, return SERVICE_FAILURE, "Set service attr failed! null ptr.");
    if (value == VALUE_ATTR_CONSOLE) {
        service->attribute |= SERVICE_ATTR_CONSOLE;
    } else if (value == VALUE_ATTR_KMSG) {
        service->attribute |= SERVICE_ATTR_KMSG;
    }
    return SERVICE_SUCCESS;
}

static int GetServiceEnv(Service *service, const cJSON *json)
{
    int envCnt = 0;
    cJSON *envArray = GetArrayItem(json, &envCnt, "env");
    INIT_CHECK(envArray != NULL, return SERVICE_SUCCESS);
    if (envCnt > 0) {
        service->env = (ServiceEnv*)calloc(envCnt, sizeof(ServiceEnv));
        INIT_ERROR_CHECK(service->env != NULL, return SERVICE_FAILURE, "calloc ServiceEnv memory failed!");
    }
    service->envCnt = envCnt;

    for (int i = 0; i < envCnt; ++i) {
        cJSON *envItem = cJSON_GetArrayItem(envArray, i);
        size_t strLen = 0;
        char *name = GetStringValue(envItem, "name", &strLen);
        INIT_ERROR_CHECK(name != NULL && strLen > 0, return SERVICE_FAILURE, "Failed to get env name failed!");
        int ret = strcpy_s(service->env[i].name, strLen + 1, name);
        INIT_INFO_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to copy env name %s", name);

        char *value = GetStringValue(envItem, "value", &strLen);
        INIT_ERROR_CHECK(value != NULL && strLen > 0, return SERVICE_FAILURE, "Failed to get value failed!");
        ret = strcpy_s(service->env[i].value, strLen + 1, value);
        INIT_INFO_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to copy env value %s", value);
    }
    return SERVICE_SUCCESS;
}

static int GetServicePeriod(const cJSON *curArrItem, Service *curServ, const char *attrName, int flag)
{
    cJSON *arrItem = cJSON_GetObjectItem(curArrItem, attrName);
    INIT_CHECK(arrItem != NULL, return SERVICE_SUCCESS);

    curServ->attribute &= ~SERVICE_ATTR_PERIOD;
    INIT_ERROR_CHECK(cJSON_IsNumber(arrItem), return SERVICE_FAILURE,
        "Service error %s is null or is not a number", curServ->name);
    curServ->attribute |= SERVICE_ATTR_PERIOD;
    uint64_t value = (uint64_t)cJSON_GetNumberValue(arrItem);
    curServ->period = value * BASE_MS_UNIT;
    return SERVICE_SUCCESS;
}

static int GetServiceCgroup(const cJSON *curArrItem, Service *service)
{
    cJSON *item = cJSON_GetObjectItem(curArrItem, "cgroup");
    service->isCgroupEnabled = false;
    INIT_CHECK(item != NULL, return SERVICE_SUCCESS);

    INIT_ERROR_CHECK(cJSON_IsBool(item), return SERVICE_FAILURE,
        "Service : %s cgroup value only support bool.", service->name);
    INIT_INFO_CHECK(cJSON_IsTrue(item), return SERVICE_SUCCESS,
        "Service : %s cgroup value is false", service->name);
    service->isCgroupEnabled = true;
    return SERVICE_SUCCESS;
}

int ParseOneService(const cJSON *curItem, Service *service)
{
    INIT_CHECK_RETURN_VALUE(curItem != NULL && service != NULL, SERVICE_FAILURE);
    int ret = GetServiceArgs(curItem, "path", MAX_PATH_ARGS_CNT, &service->pathArgs);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get path for service %s", service->name);
    if ((service->pathArgs.count > 0) && IsForbidden(service->pathArgs.argv[0])) {
        INIT_LOGE("Service %s is forbidden.", service->name);
        return SERVICE_FAILURE;
    }
#ifdef ASAN_DETECTOR
    SetServicePathWithAsan(service);
#endif
    ret = GetUid(cJSON_GetObjectItem(curItem, UID_STR_IN_CFG), &service->servPerm.uID);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get uid for service %s", service->name);
    ret = GetServiceGids(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get gid for service %s", service->name);

    ret = GetServiceAttr(curItem, service, ONCE_STR_IN_CFG, SERVICE_ATTR_ONCE, NULL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get once flag for service %s", service->name);
    ret = GetServiceAttr(curItem, service, IMPORTANT_STR_IN_CFG, SERVICE_ATTR_IMPORTANT, SetImportantValue);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get import flag for service %s", service->name);
    ret = GetCritical(curItem, service, CRITICAL_STR_IN_CFG, SERVICE_ATTR_CRITICAL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get critical flag for service %s", service->name);
    ret = GetServiceAttr(curItem, service, DISABLED_STR_IN_CFG, SERVICE_ATTR_DISABLED, NULL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get disabled flag for service %s", service->name);
    ret = GetServiceAttr(curItem, service, CONSOLE_STR_IN_CFG, SERVICE_ATTR_CONSOLE, SetConsoleValue);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get console for service %s", service->name);
    ret = GetServiceAttr(curItem, service, "notify-state", SERVICE_ATTR_NOTIFY_STATE, NULL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get notify-state for service %s", service->name);
    ret = GetServiceAttr(curItem, service, MODULE_UPDATE_STR_IN_CFG, SERVICE_ATTR_MODULE_UPDATE, NULL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get module-update for service %s", service->name);

    ParseOneServiceArgs(curItem, service);
    ret = GetServiceEnv(service, curItem);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get env for service %s", service->name);
    ret = GetServicePeriod(curItem, service, PERIOD_STR_IN_CFG, SERVICE_ATTR_PERIOD);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get period for service %s", service->name);
    ret = GetServiceSandbox(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get sandbox for service %s", service->name);
    ret = InitServiceCaps(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get caps for service %s", service->name);
    ret = GetServiceOnDemand(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get ondemand flag for service %s", service->name);
    ret = GetServiceSetuid(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get setuid flag for service %s", service->name);
    ret = GetServiceMode(service, curItem);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get start/end mode for service %s", service->name);
    ret = GetServiceJobs(service, cJSON_GetObjectItem(curItem, "jobs"));
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get jobs for service %s", service->name);
    ret = GetServiceCgroup(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get cgroup for service %s", service->name);
    return ret;
}

#if defined(ENABLE_HOOK_MGR)
/**
 * Service Config File Parse Hooking
 */
static int ServiceParseHookWrapper(const HOOK_INFO *hookInfo, void *executionContext)
{
    SERVICE_PARSE_CTX *serviceParseContext = (SERVICE_PARSE_CTX *)executionContext;
    ServiceParseHook realHook = (ServiceParseHook)hookInfo->hookCookie;

    realHook(serviceParseContext);
    return 0;
};

int InitAddServiceParseHook(ServiceParseHook hook)
{
    HOOK_INFO info;

    info.stage = INIT_SERVICE_PARSE;
    info.prio = 0;
    info.hook = ServiceParseHookWrapper;
    info.hookCookie = (void *)hook;

    return HookMgrAddEx(GetBootStageHookMgr(), &info);
}

static void ParseServiceHookExecute(const char *name, const cJSON *serviceNode)
{
    SERVICE_PARSE_CTX context;

    context.serviceName = name;
    context.serviceNode = serviceNode;

    (void)HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_PARSE, (void *)(&context), NULL);
}
#endif

static void ProcessConsoleEvent(const WatcherHandle handler, int fd, uint32_t *events, const void *context)
{
    Service *service = (Service *)context;
    LE_RemoveWatcher(LE_GetDefaultLoop(), (WatcherHandle)handler);
    if (fd < 0 || service == NULL) {
        INIT_LOGE("Process console event with invalid arguments");
        return;
    }
    // Since we've got event from console device
    // the fd related to '/dev/console' does not need anymore, close it.
    close(fd);
    if (strcmp(service->name, "console") != 0) {
        INIT_LOGE("Process console event with invalid service %s, only console service should do this", service->name);
        return;
    }

    if (ServiceStart(service, &service->pathArgs) != SERVICE_SUCCESS) {
        INIT_LOGE("Start console service failed");
    }
    return;
}

static int AddFileDescriptorToWatcher(int fd, Service *service)
{
    if (fd < 0 || service == NULL) {
        return -1;
    }

    WatcherHandle watcher = NULL;
    LE_WatchInfo info = {};
    info.fd = fd;
    info.flags = 0; // WATCHER_ONCE;
    info.events = EVENT_READ;
    info.processEvent = ProcessConsoleEvent;
    int ret = LE_StartWatcher(LE_GetDefaultLoop(), &watcher, &info, service);
    if (ret != LE_SUCCESS) {
        INIT_LOGE("Failed to watch console device for service \' %s \'", service->name);
        return -1;
    }
    return 0;
}

int WatchConsoleDevice(Service *service)
{
    if (service == NULL) {
        return -1;
    }

    int fd = open("/dev/console", O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT) {
            INIT_LOGW("/dev/console is not exist, wait for it...");
            WaitForFile("/dev/console", WAIT_MAX_SECOND);
        } else {
            INIT_LOGE("Failed to open /dev/console, err = %d", errno);
            return -1;
        }
        fd = open("/dev/console", O_RDWR | O_NOCTTY | O_CLOEXEC);
        if (fd < 0) {
            INIT_LOGW("Failed to open /dev/console after try 1 time, err = %d", errno);
            return -1;
        }
    }

    if (AddFileDescriptorToWatcher(fd, service) < 0) {
        close(fd);
        return -1;
    }
    return 0;
}

void ParseAllServices(const cJSON *fileRoot, const ConfigContext *context)
{
    int servArrSize = 0;
    cJSON *serviceArr = GetArrayItem(fileRoot, &servArrSize, SERVICES_ARR_NAME_IN_JSON);
    INIT_CHECK(serviceArr != NULL, return);

    size_t strLen = 0;
    for (int i = 0; i < servArrSize; ++i) {
        cJSON *curItem = cJSON_GetArrayItem(serviceArr, i);
        char *fieldStr = GetStringValue(curItem, "name", &strLen);
        if (fieldStr == NULL) {
            INIT_LOGE("Failed to get service name");
            continue;
        }
        Service *service = GetServiceByName(fieldStr);
        if (service == NULL) {
            service = AddService(fieldStr);
            if (service == NULL) {
                INIT_LOGE("Failed to add service name %s", fieldStr);
                continue;
            }
        } else {
            INIT_LOGI("Service %s already exists, updating.", fieldStr);
#ifndef __MUSL__
            continue;
#endif
        }

        if (context != NULL) {
            service->context.type = context->type;
        }
        int ret = ParseOneService(curItem, service);
        if (ret != SERVICE_SUCCESS) {
            INIT_LOGE("Service error %s parse config error.", service->name);
            service->lastErrno = INIT_ECFG;
            continue;
        }
        ret = ParseServiceSocket(curItem, service);
        INIT_CHECK(ret == 0, FreeServiceSocket(service->socketCfg); service->socketCfg = NULL);
        ret = ParseServiceFile(curItem, service);
        INIT_CHECK(ret == 0, FreeServiceFile(service->fileCfg); service->fileCfg = NULL);
        // Watch "/dev/console" node for starting console service ondemand.
        if ((strcmp(service->name, "console") == 0) && IsOnDemandService(service)) {
            if (WatchConsoleDevice(service) < 0) {
                INIT_LOGW("Failed to watch \'/dev/console\' device");
            }
        }
#if defined(ENABLE_HOOK_MGR)
        /*
         * Execute service parsing hooks
         */
        ParseServiceHookExecute(fieldStr, curItem);
#endif

        ret = GetCmdLinesFromJson(cJSON_GetObjectItem(curItem, "onrestart"), &service->restartArg);
        INIT_CHECK(ret == SERVICE_SUCCESS, service->restartArg = NULL);
    }
}

static Service *GetServiceByExtServName(const char *fullServName, ServiceArgs *extraArgs)
{
    INIT_ERROR_CHECK(fullServName != NULL, return NULL, "Failed get parameters");
    Service *service = GetServiceByName(fullServName);
    if (service != NULL) { // none parameter in fullServName
        return service;
    }
    char *tmpServName = strdup(fullServName);
    INIT_ERROR_CHECK(tmpServName != NULL, return NULL, "Failed dup parameters");
    char *dstPtr[MAX_PATH_ARGS_CNT] = {NULL};
    int returnCount = SplitString(tmpServName, "|", dstPtr, MAX_PATH_ARGS_CNT);
    if (returnCount == 0) {
        INIT_LOGE("Service error %s start service bt ext parameter .", fullServName);
        free(tmpServName);
        return NULL;
    }
    INIT_LOGI("Service info %s start service bt ext parameter %s.", dstPtr[0], fullServName);
    service = GetServiceByName(dstPtr[0]);
    if (service == NULL) {
        free(tmpServName);
        return NULL;
    }
    extraArgs->count = service->pathArgs.count + returnCount - 1;
    extraArgs->argv = (char **)calloc(extraArgs->count + 1, sizeof(char *));
    INIT_ERROR_CHECK(extraArgs->argv != NULL, free(tmpServName);
        return NULL, "Failed calloc err=%d", errno);
    int argc;
    for (argc = 0; argc < (service->pathArgs.count - 1); argc++) {
        extraArgs->argv[argc] = strdup(service->pathArgs.argv[argc]);
        INIT_ERROR_CHECK(extraArgs->argv[argc] != NULL, free(tmpServName);
            return NULL, "Failed dup path");
    }
    int extArgc;
    for (extArgc = 0; extArgc < (returnCount - 1); extArgc++) {
        extraArgs->argv[extArgc + argc] = strdup(dstPtr[extArgc + 1]);
        INIT_ERROR_CHECK(extraArgs->argv[extArgc + argc] != NULL, free(tmpServName);
            return NULL, "Failed dup path");
    }
    extraArgs->argv[extraArgs->count] = NULL;
    free(tmpServName);
    return service;
}

void StartServiceByName(const char *servName)
{
    ServiceArgs extraArgs = { 0 };
    Service *service = GetServiceByName(servName);
    if (service == NULL) {
        service = GetServiceByExtServName(servName, &extraArgs);
    }
    INIT_ERROR_CHECK(service != NULL, FreeStringVector(extraArgs.argv, extraArgs.count);
        return, "Cannot find service %s.service count %d", servName, g_serviceSpace.serviceCount);

    ServiceArgs *pathArgs = &service->pathArgs;
    if (extraArgs.count != 0) {
        pathArgs = &extraArgs;
    }
    if (ServiceStart(service, pathArgs) != SERVICE_SUCCESS) {
        INIT_LOGE("Service %s start failed!", servName);
    }
    // After starting, clear the extra parameters.
    FreeStringVector(extraArgs.argv, extraArgs.count);
    return;
}

void StopServiceByName(const char *servName)
{
    Service *service = GetServiceByName(servName);
    INIT_ERROR_CHECK(service != NULL, return, "Cannot find service %s.", servName);

    if (ServiceStop(service) != SERVICE_SUCCESS) {
        INIT_LOGE("Service %s stop failed!", servName);
    }
    return;
}

void TermServiceByName(const char *servName)
{
    Service *service = GetServiceByName(servName);
    INIT_ERROR_CHECK(service != NULL, return, "Cannot find service %s.", servName);
    if (ServiceTerm(service) != SERVICE_SUCCESS) {
        INIT_LOGE("Service %s term failed!", servName);
    }
    return;
}

void StopAllServices(int flags, const char **exclude, int size,
    int (*filter)(const Service *service, const char **exclude, int size))
{
    Service *service = GetServiceByName("appspawn");
    if (service != NULL && service->pid > 0) { // notify appspawn stop
#ifndef STARTUP_INIT_TEST
        kill(service->pid, SIGTERM);
        waitpid(service->pid, 0, 0);
        service->pid = -1;
#endif
    }

    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        Service *service = node->data.service;
        if (service == NULL) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        INIT_LOGI("StopAllServices stop service %s ", service->name);
        if (filter != NULL && filter(service, exclude, size) != 0) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        service->attribute |= (flags & SERVICE_ATTR_INVALID);
        int ret = ServiceStop(service);
        if (ret != SERVICE_SUCCESS) {
            INIT_LOGE("Service %s stop failed!", service->name);
        }
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }

    INIT_TIMING_STAT cmdTimer;
    (void)clock_gettime(CLOCK_MONOTONIC, &cmdTimer.startTime);
    long long count = 1;
    while (count > 0) {
        usleep(INTERVAL_PER_WAIT);
        count++;
        (void)clock_gettime(CLOCK_MONOTONIC, &cmdTimer.endTime);
        long long diff = InitDiffTime(&cmdTimer);
        if (diff > REBOOT_WAIT_TIME) {
            count = 0;
        }
    }
    INIT_LOGI("StopAllServices end");
#if defined(ENABLE_HOOK_MGR)
    HookMgrExecute(GetBootStageHookMgr(), INIT_ALL_SERVICES_REAP, NULL, NULL);
#endif
}

Service *GetServiceByPid(pid_t pid)
{
    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        Service *service = node->data.service;
        if (service != NULL && service->pid == pid) {
            return service;
        }
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
    return NULL;
}

Service *GetServiceByName(const char *servName)
{
    INIT_ERROR_CHECK(servName != NULL, return NULL, "Failed get servName");
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_SERVICES, servName);
    if (groupNode != NULL) {
        return groupNode->data.service;
    }
    return NULL;
}

void LoadAccessTokenId(void)
{
    GetAccessToken();
}

int GetKillServiceSig(const char *name)
{
    if (strcmp(name, "appspawn") == 0 || strcmp(name, "nwebspawn") == 0) {
        return SIGTERM;
    }
    return SIGKILL;
}

int GetServiceGroupIdByPid(pid_t pid, gid_t *gids, uint32_t gidSize)
{
    Service *service = GetServiceByPid(pid);
    if (service != NULL) {
        int ret = memcpy_s(gids, gidSize * sizeof(gid_t),
            service->servPerm.gIDArray, service->servPerm.gIDCnt * sizeof(gid_t));
        INIT_ERROR_CHECK(ret == 0, return 0, "Failed get copy gids");
        return service->servPerm.gIDCnt;
    }
    return 0;
}