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
#include <unistd.h>

#include "cJSON.h"
#include "init.h"
#include "init_group_manager.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_service_file.h"
#include "init_service_socket.h"
#include "init_utils.h"
#include "securec.h"
#ifdef WITH_SELINUX
#   include "init_selinux_param.h"
#endif // WITH_SELINUX

// All serivce processes that init will fork+exec.
static ServiceSpace g_serviceSpace = { 0 };
static const int CRITICAL_DEFAULT_CRASH_TIME = 20;
// maximum number of crashes within time CRITICAL_DEFAULT_CRASH_TIME for one service
static const int CRITICAL_DEFAULT_CRASH_COUNT =  4;
static const int CRITICAL_CONFIG_ARRAY_LEN = 3;

#ifdef OHOS_SERVICE_DUMP
static void DumpServiceArgs(const char *info, const ServiceArgs *args)
{
    INIT_LOGI("\t\t%s count %d", info, args->count);
    for (int j = 0; j < args->count; j++) {
        if (args->argv[j] != NULL) {
            INIT_LOGI("\t\tinfo [%d] %s", j, args->argv[j]);
        }
    }
}

void DumpAllServices()
{
    INIT_LOGI("Ready to dump all services:");
    INIT_LOGI("total service number: %d", g_serviceSpace.serviceCount);
    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        if (node->data.service == NULL) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        Service *service = node->data.service;
        INIT_LOGI("\tservice name: [%s]", service->name);
        INIT_LOGI("\tservice pid: [%d]", service->pid);
        INIT_LOGI("\tservice crashCnt: [%d]", service->crashCnt);
        INIT_LOGI("\tservice attribute: [%d]", service->attribute);
        INIT_LOGI("\tservice importance: [%d]", service->importance);
        INIT_LOGI("\tservice perms uID [%d]", service->servPerm.uID);
        DumpServiceArgs("path arg", &service->pathArgs);
        DumpServiceArgs("writepid file", &service->writePidArgs);

        INIT_LOGI("\tservice perms groupId %d", service->servPerm.gIDCnt);
        for (int i = 0; i < service->servPerm.gIDCnt; i++) {
            INIT_LOGI("\t\tservice perms groupId %d", service->servPerm.gIDArray[i]);
        }

        INIT_LOGI("\tservice perms capability %d", service->servPerm.capsCnt);
        for (int i = 0; i < (int)service->servPerm.capsCnt; i++) {
            INIT_LOGI("\t\tservice perms capability %d", service->servPerm.caps[i]);
        }
        if (service->restartArg != NULL) {
            for (int j = 0; j < service->restartArg->cmdNum; j++) {
                CmdLine *cmd = &service->restartArg->cmds[j];
                INIT_LOGI("\t\tcmd arg: %s %s", GetCmdKey(cmd->cmdIndex), cmd->cmdContent);
            }
        }
        if (service->socketCfg != NULL) {
            INIT_LOGI("\tservice socket name: %s", service->socketCfg->name);
            INIT_LOGI("\tservice socket type: %d", service->socketCfg->type);
            INIT_LOGI("\tservice socket uid: %d", service->socketCfg->uid);
            INIT_LOGI("\tservice socket gid: %d", service->socketCfg->gid);
        }
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
    INIT_LOGI("Dump all services finished");
}
#endif

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
    while (sockopt != NULL) {
        ServiceSocket *tmp = sockopt;
        if (tmp->sockFd >= 0) {
            close(tmp->sockFd);
            tmp->sockFd = -1;
        }
        sockopt = sockopt->next;
        free(tmp);
    }
    return;
}

static Service *AddService(const char *name)
{
    // check service in group
    if (CheckNodeValid(NODE_TYPE_SERVICES, name) != 0) {
        return NULL;
    }
    InitGroupNode *node = AddGroupNode(NODE_TYPE_SERVICES, name);
    if (node == NULL) {
        return NULL;
    }
    Service *service = (Service *)calloc(1, sizeof(Service));
    INIT_ERROR_CHECK(service != NULL, return NULL, "Failed to malloc for service");
    node->data.service = service;
    service->name = node->name;
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
    return;
}

void ReleaseService(Service *service)
{
    if (service == NULL) {
        return;
    }
    FreeServiceArg(&service->pathArgs);
    FreeServiceArg(&service->writePidArgs);
    FreeServiceArg(&service->capsArgs);

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

    g_serviceSpace.serviceCount--;
    InitGroupNode *groupNode = GetGroupNode(NODE_TYPE_SERVICES, service->name);
    if (groupNode != NULL) {
        groupNode->data.service = NULL;
    }
    free(service);
}

static int GetStringItem(const cJSON *json, const char *name, char *buffer, int buffLen)
{
    INIT_ERROR_CHECK(json != NULL, return SERVICE_FAILURE, "Invalid json for %s", name);
    char *fieldStr = cJSON_GetStringValue(cJSON_GetObjectItem(json, name));
    if (fieldStr == NULL) {
        return SERVICE_FAILURE;
    }
    size_t strLen = strlen(fieldStr);
    INIT_ERROR_CHECK((strLen != 0) && (strLen <= (size_t)buffLen), return SERVICE_FAILURE,
        "Invalid str filed %s for %s", fieldStr, name);
    return strcpy_s(buffer, buffLen, fieldStr);
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
    INIT_ERROR_CHECK((count > 0) && (count < maxCount), return SERVICE_FAILURE, "Array size = %d is wrong", count);

    args->argv = (char **)malloc((count + 1) * sizeof(char *));
    INIT_ERROR_CHECK(args->argv != NULL, return SERVICE_FAILURE, "Failed to malloc for argv");
    for (int i = 0; i < count + 1; ++i) {
        args->argv[i] = NULL;
    }
     // ServiceArgs have a variety of uses, some requiring a NULL ending, some not
    if (strcmp(name, D_CAPS_STR_IN_CFG) != 0) {
        args->count = count + 1;
    } else {
        args->count = count;
    }
    for (int i = 0; i < count; ++i) {
        char *curParam = cJSON_GetStringValue(cJSON_GetArrayItem(obj, i));
        INIT_ERROR_CHECK(curParam != NULL, return SERVICE_FAILURE, "Invalid arg %d", i);
        INIT_ERROR_CHECK(strlen(curParam) <= MAX_ONE_ARG_LEN, return SERVICE_FAILURE, "Arg %s is tool long", curParam);
        args->argv[i] = strdup(curParam);
        INIT_ERROR_CHECK(args->argv[i] != NULL, return SERVICE_FAILURE, "Failed to dupstring %s", curParam);
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
    }
    INIT_CHECK_RETURN_VALUE(*uid != (uid_t)(-1), SERVICE_FAILURE);
    return SERVICE_SUCCESS;
}

static int GetServiceGids(const cJSON *curArrItem, Service *curServ)
{
    int gidCount;
    cJSON *arrItem = cJSON_GetObjectItemCaseSensitive(curArrItem, GID_STR_IN_CFG);
    if (!cJSON_IsArray(arrItem)) {
        gidCount = 1;
    } else {
        gidCount = cJSON_GetArraySize(arrItem);
    }
    INIT_ERROR_CHECK((gidCount != 0) && (gidCount <= NGROUPS_MAX + 1), return SERVICE_FAILURE,
        "Invalid gid count %d", gidCount);
    curServ->servPerm.gIDArray = (gid_t *)malloc(sizeof(gid_t) * gidCount);
    INIT_ERROR_CHECK(curServ->servPerm.gIDArray != NULL, return SERVICE_FAILURE, "Failed to malloc");
    curServ->servPerm.gIDCnt = gidCount;

    gid_t gid;
    if (!cJSON_IsArray(arrItem)) {
        int ret = GetUid(arrItem, &gid);
        INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to uid");
        curServ->servPerm.gIDArray[0] = gid;
        return SERVICE_SUCCESS;
    }

    for (int i = 0; i < gidCount; ++i) {
        cJSON *item = cJSON_GetArrayItem(arrItem, i);
        int ret = GetUid(item, &gid);
        if (ret != 0) {
            curServ->servPerm.gIDArray[i] = 0;
            continue;
        }
        curServ->servPerm.gIDArray[i] = gid;
    }
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
        "%s is null or is not a number, service name is %s", attrName, curServ->name);

    int value = (int)cJSON_GetNumberValue(filedJ);
    if (processAttr == NULL) {
        curServ->attribute &= ~flag;
        if (value == 1) {
            curServ->attribute |= flag;
        }
        return 0;
    }
    return processAttr(curServ, attrName, value, flag);
}

static int AddServiceSocket(cJSON *json, Service *service)
{
    char *opt[SOCK_OPT_NUMS] = {
        NULL,
    };
    INIT_CHECK_RETURN_VALUE(cJSON_IsString(json) && cJSON_GetStringValue(json), SERVICE_FAILURE);
    char *sockStr = cJSON_GetStringValue(json);
    int num = SplitString(sockStr, " ", opt, SOCK_OPT_NUMS);
    INIT_CHECK_RETURN_VALUE(num == SOCK_OPT_NUMS, SERVICE_FAILURE);

    if (opt[SERVICE_SOCK_NAME] == NULL || opt[SERVICE_SOCK_TYPE] == NULL || opt[SERVICE_SOCK_PERM] == NULL ||
        opt[SERVICE_SOCK_UID] == NULL || opt[SERVICE_SOCK_GID] == NULL || opt[SERVICE_SOCK_SETOPT] == NULL) {
        INIT_LOGE("Invalid socket opt");
        return SERVICE_FAILURE;
    }

    ServiceSocket *sockopt = (ServiceSocket *)calloc(1, sizeof(ServiceSocket) + strlen(opt[SERVICE_SOCK_NAME]) + 1);
    INIT_INFO_CHECK(sockopt != NULL, return SERVICE_FAILURE, "Failed to malloc for socket %s", opt[SERVICE_SOCK_NAME]);
    sockopt->sockFd = -1;
    int ret = strcpy_s(sockopt->name, strlen(opt[SERVICE_SOCK_NAME]) + 1, opt[SERVICE_SOCK_NAME]);
    INIT_INFO_CHECK(ret == 0, free(sockopt);
        return SERVICE_FAILURE, "Failed to copy socket name %s", opt[SERVICE_SOCK_NAME]);

    sockopt->type = SOCK_SEQPACKET;
    if (strncmp(opt[SERVICE_SOCK_TYPE], "stream", strlen(opt[SERVICE_SOCK_TYPE])) == 0) {
        sockopt->type = SOCK_STREAM;
    } else if (strncmp(opt[SERVICE_SOCK_TYPE], "dgram", strlen(opt[SERVICE_SOCK_TYPE])) == 0) {
        sockopt->type = SOCK_DGRAM;
    }
    sockopt->perm = strtoul(opt[SERVICE_SOCK_PERM], 0, OCTAL_BASE);
    sockopt->uid = DecodeUid(opt[SERVICE_SOCK_UID]);
    sockopt->gid = DecodeUid(opt[SERVICE_SOCK_GID]);
    if (sockopt->uid == (uid_t)-1 || sockopt->gid == (uid_t)-1) {
        free(sockopt);
        sockopt = NULL;
        INIT_LOGE("Invalid uid or gid");
        return SERVICE_FAILURE;
    }
    sockopt->passcred = false;
    if (strncmp(opt[SERVICE_SOCK_SETOPT], "passcred", strlen(opt[SERVICE_SOCK_SETOPT])) == 0) {
        sockopt->passcred = true;
    }
    sockopt->watcher = NULL;
    sockopt->sockFd = -1;
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
        ret = CreateServiceSocket(curServ);
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
    if (num != FILE_OPT_NUMS) {
        return SERVICE_FAILURE;
    }
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
        return SERVICE_FAILURE;
    }
    fileOpt->perm = strtoul(opt[SERVICE_FILE_PERM], 0, OCTAL_BASE);
    fileOpt->uid = DecodeUid(opt[SERVICE_FILE_UID]);
    fileOpt->gid = DecodeUid(opt[SERVICE_FILE_GID]);
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

static bool IsServiceInMainStrap(Service *curServ)
{
    char *mainServiceList[] = {
        "appspawn", "udevd",  "samgr",      "multimodalinput", "weston",         "installs",
        "hiview",   "hilogd", "hdf_devmgr", "distributedsche", "softbus_server", "foundation"
    };
    unsigned int length = ARRAY_LENGTH(mainServiceList);
    for (unsigned int i = 0; i < length; ++i) {
        INIT_INFO_CHECK(strncmp(curServ->name, mainServiceList[i], strlen(mainServiceList[i])) != 0, return true,
            "%s must be main service", curServ->name);
    }
    return false;
}

static int GetDynamicService(const cJSON *curArrItem, Service *curServ)
{
    cJSON *item = cJSON_GetObjectItem(curArrItem, "dynamic");
    if (item == NULL) {
        return SERVICE_SUCCESS;
    }

    INIT_ERROR_CHECK(cJSON_IsBool(item), return SERVICE_FAILURE,
        "Service : %s dynamic value only support bool.", curServ->name);
    bool isDynamic = (bool)cJSON_GetNumberValue(item);
    INIT_INFO_CHECK(isDynamic, return SERVICE_SUCCESS,
        "Service : %s dynamic value is false, it will be started with init.", curServ->name);
    INIT_CHECK_RETURN_VALUE(!IsServiceInMainStrap(curServ), SERVICE_SUCCESS);
    INIT_LOGI("%s is dynamic service", curServ->name);

    curServ->attribute |= SERVICE_ATTR_DYNAMIC;
    curServ->attribute |= SERVICE_ATTR_ONCE;
    return SERVICE_SUCCESS;
}

static int GetServiceOnDemand(const cJSON *curArrItem, Service *curServ)
{
    cJSON *item = cJSON_GetObjectItem(curArrItem, "ondemand");
    if (item == NULL) {
        return SERVICE_SUCCESS;
    }

    INIT_ERROR_CHECK(cJSON_IsBool(item), return SERVICE_FAILURE,
        "Service : %s ondemand value only support bool.", curServ->name);
    bool isOnDemand = (bool)cJSON_GetNumberValue(item);
    INIT_INFO_CHECK(isOnDemand, return SERVICE_SUCCESS,
        "Service : %s ondemand value is false, it will be manage socket by itself", curServ->name);

    curServ->attribute |= SERVICE_ATTR_ONDEMAND;
    return SERVICE_SUCCESS;
}

static int CheckServiceKeyName(const cJSON *curService)
{
    char *cfgServiceKeyList[] = {
        "name", "path", "uid", "gid", "once", "importance", "caps", "disabled",
        "writepid", "critical", "socket", "console", "dynamic", "file", "ondemand",
        "d-caps", "apl", "jobs", "start-mode", "end-mode",
#ifdef WITH_SELINUX
        SECON_STR_IN_CFG,
#endif // WITH_SELINUX
    };
    INIT_CHECK_RETURN_VALUE(curService != NULL, SERVICE_FAILURE);
    cJSON *child = curService->child;
    INIT_CHECK_RETURN_VALUE(child != NULL, SERVICE_FAILURE);
    while (child != NULL) {
        int i = 0;
        int keyListSize = ARRAY_LENGTH(cfgServiceKeyList);
        for (; i < keyListSize; i++) {
            if (strcmp(child->string, cfgServiceKeyList[i]) == 0) {
                break;
            }
        }
        if (i < keyListSize) {
            child = child->next;
        } else {
            INIT_LOGE("CheckServiceKeyName, key name %s is not found. error.", child->string);
            return SERVICE_FAILURE;
        }
    }
    return SERVICE_SUCCESS;
}

static int GetServiceMode(Service *service, const cJSON *json)
{
    const InitArgInfo startModeMap[] = {
        {"condition", START_MODE_CONDITION},
        {"boot", START_MODE_BOOT},
        {"normal", START_MODE_NARMAL}
    };
    const InitArgInfo endModeMap[] = {
        {"pre-fork", END_PRE_FORK},
        {"after-fork", END_AFTER_FORK},
        {"after-exec", END_AFTER_EXEC},
        {"ready", END_RECV_READY}
    };
    service->startMode = START_MODE_NARMAL;
    service->endMode = END_AFTER_EXEC;
    char *value = cJSON_GetStringValue(cJSON_GetObjectItem(json, "start-mode"));
    if (value != NULL) {
        service->startMode = GetMapValue(value,
            (InitArgInfo *)&startModeMap, (int)ARRAY_LENGTH(startModeMap), START_MODE_NARMAL);
    }
    value = cJSON_GetStringValue(cJSON_GetObjectItem(json, "end-mode"));
    if (value != NULL) {
        service->endMode = GetMapValue(value,
            (InitArgInfo *)&endModeMap, (int)ARRAY_LENGTH(endModeMap), END_AFTER_EXEC);
    }
    return 0;
}

static int GetServiceJobs(Service *service, cJSON *json)
{
    const char *jobTypes[] = {
        "on-boot", "on-start", "on-stop", "on-restart"
    };
    for (int i = 0; i < (int)ARRAY_LENGTH(jobTypes); i++) {
        char *jobName = cJSON_GetStringValue(cJSON_GetObjectItem(json, jobTypes[i]));
        if (jobName != NULL) {
            service->serviceJobs.jobsName[i] = strdup(jobName);
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

int ParseOneService(const cJSON *curItem, Service *service)
{
    INIT_CHECK_RETURN_VALUE(curItem != NULL && service != NULL, SERVICE_FAILURE);
    int ret = 0;
#ifdef WITH_SELINUX
    ret = GetStringItem(curItem, SECON_STR_IN_CFG, service->secon, MAX_SECON_LEN);
    INIT_CHECK_ONLY_ELOG(ret == 0, "GetServiceSecon %s section not found, skip", SECON_STR_IN_CFG);
#endif // WITH_SELINUX
    ret = GetServiceArgs(curItem, "path", MAX_PATH_ARGS_CNT, &service->pathArgs);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get path for service %s", service->name);
    if ((service->pathArgs.count > 0) && IsForbidden(service->pathArgs.argv[0])) {
        INIT_LOGE("Service %s is forbidden.", service->name);
        return SERVICE_FAILURE;
    }
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
    ret = GetServiceAttr(curItem, service, CONSOLE_STR_IN_CFG, SERVICE_ATTR_CONSOLE, NULL);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get console for service %s", service->name);

    (void)GetServiceArgs(curItem, "writepid", MAX_WRITEPID_FILES, &service->writePidArgs);
    (void)GetServiceArgs(curItem, D_CAPS_STR_IN_CFG, MAX_WRITEPID_FILES, &service->capsArgs);
    (void)GetStringItem(curItem, APL_STR_IN_CFG, service->apl, MAX_APL_NAME);
    ret = GetServiceCaps(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get caps for service %s", service->name);
    ret = GetDynamicService(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get dynamic flag for service %s", service->name);
    ret = GetServiceOnDemand(curItem, service);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get ondemand flag for service %s", service->name);
    ret = GetServiceMode(service, curItem);
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get start/end mode for service %s", service->name);
    ret = GetServiceJobs(service, cJSON_GetObjectItem(curItem, "jobs"));
    INIT_ERROR_CHECK(ret == 0, return SERVICE_FAILURE, "Failed to get jobs for service %s", service->name);
    return ret;
}

void ParseAllServices(const cJSON *fileRoot)
{
    int servArrSize = 0;
    cJSON *serviceArr = GetArrayItem(fileRoot, &servArrSize, SERVICES_ARR_NAME_IN_JSON);
    INIT_CHECK(serviceArr != NULL, return);

    INIT_ERROR_CHECK(servArrSize <= MAX_SERVICES_CNT_IN_FILE, return,
        "Too many services[cnt %d] detected, should not exceed %d.",
        servArrSize, MAX_SERVICES_CNT_IN_FILE);

    char serviceName[MAX_SERVICE_NAME] = {};
    for (int i = 0; i < servArrSize; ++i) {
        cJSON *curItem = cJSON_GetArrayItem(serviceArr, i);
        int ret = GetStringItem(curItem, "name", serviceName, MAX_SERVICE_NAME);
        if (ret != 0) {
            INIT_LOGE("Failed to get service name %s", serviceName);
            continue;
        }
        if (CheckServiceKeyName(curItem) != SERVICE_SUCCESS) { // invalid service
            INIT_LOGE("Invalid service name %s", serviceName);
            continue;
        }
        Service *service = GetServiceByName(serviceName);
        if (service != NULL) {
            INIT_LOGE("Service name %s has been exist", serviceName);
            continue;
        }
        service = AddService(serviceName);
        if (service == NULL) {
            INIT_LOGE("Failed to create service name %s", serviceName);
            continue;
        }

        ret = ParseOneService(curItem, service);
        if (ret != SERVICE_SUCCESS) {
            ReleaseService(service);
            service = NULL;
            continue;
        }
        if (ParseServiceSocket(curItem, service) != SERVICE_SUCCESS) {
            FreeServiceSocket(service->socketCfg);
            service->socketCfg = NULL;
        }
        if (ParseServiceFile(curItem, service) != SERVICE_SUCCESS) {
            FreeServiceFile(service->fileCfg);
            service->fileCfg = NULL;
        }
        ret = GetCmdLinesFromJson(cJSON_GetObjectItem(curItem, "onrestart"), &service->restartArg);
        INIT_CHECK(ret == SERVICE_SUCCESS, service->restartArg = NULL);
    }
}

static Service *GetServiceByExtServName(const char *fullServName)
{
    INIT_ERROR_CHECK(fullServName != NULL, return NULL, "Failed get parameters");
    char *tmpServName = strdup(fullServName);
    char *dstPtr[MAX_PATH_ARGS_CNT] = {NULL};
    int returnCount = SplitString(tmpServName, "|", dstPtr, MAX_PATH_ARGS_CNT);
    if (returnCount == 0) {
        free(tmpServName);
        return NULL;
    }
    Service *service = GetServiceByName(dstPtr[0]);
    if (service == NULL) {
        free(tmpServName);
        return NULL;
    }
    service->extraArgs.count = service->pathArgs.count + returnCount - 1;
    service->extraArgs.argv = (char **)calloc(service->extraArgs.count + 1, sizeof(char *));
    INIT_ERROR_CHECK(service->extraArgs.argv != NULL, free(tmpServName);
        return NULL, "Failed calloc err=%d", errno);
    int argc;
    for (argc = 0; argc < (service->pathArgs.count - 1); argc++) {
        service->extraArgs.argv[argc] = strdup(service->pathArgs.argv[argc]);
    }
    int extArgc;
    for (extArgc = 0; extArgc < (returnCount - 1); extArgc++) {
        service->extraArgs.argv[extArgc + argc] = strdup(dstPtr[extArgc + 1]);
    }
    for (int i = 0; i < service->extraArgs.count - 1; i++) {
        INIT_LOGI("service->extraArgs.argv[%d] is %s", i, service->extraArgs.argv[i]);
    }
    service->extraArgs.argv[service->extraArgs.count] = NULL;
    free(tmpServName);
    return service;
}

void StartServiceByName(const char *servName, bool checkDynamic)
{
    INIT_LOGE("StartServiceByName Service %s", servName);
    Service *service = GetServiceByName(servName);
    if (service == NULL) {
        service = GetServiceByExtServName(servName);
    }
    INIT_ERROR_CHECK(service != NULL, return, "Cannot find service %s.", servName);

    if (checkDynamic && (service->attribute & SERVICE_ATTR_DYNAMIC)) {
        INIT_LOGI("%s is dynamic service.", servName);
        NotifyServiceChange(servName, "stopped");
        return;
    }
    if (ServiceStart(service) != SERVICE_SUCCESS) {
        INIT_LOGE("Service %s start failed!", servName);
    }
    // After starting, clear the extra parameters.
    FreeStringVector(service->extraArgs.argv, service->extraArgs.count);
    service->extraArgs.argv = NULL;
    service->extraArgs.count = 0;
    return;
}

void StopServiceByName(const char *servName)
{
    Service *service = GetServiceByName(servName);
    INIT_ERROR_CHECK(service != NULL, return, "Cannot find service %s.", servName);

    if (ServiceStop(service) != SERVICE_SUCCESS) {
        INIT_LOGE("Service %s start failed!", servName);
    }
    return;
}

void StopAllServices(int flags)
{
    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        Service *service = node->data.service;
        if (service != NULL) {
            service->attribute |= flags;
            int ret = ServiceStop(service);
            if (ret != SERVICE_SUCCESS) {
                INIT_LOGE("Service %s stop failed!", service->name);
            }
        }
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
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

void StartAllServices(int startMode)
{
    INIT_LOGI("StartAllServices %d", startMode);
#ifdef SUPPORT_GROUP_SERVICE_START
    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        Service *service = node->data.service;
        if (service == NULL || service->startMode != startMode) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        if (IsOnDemandService(service)) {
            if (CreateServiceSocket(service) != 0) {
                INIT_LOGE("service %s exit! create socket failed!", service->name);
            }
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        if (service->attribute & SERVICE_ATTR_DYNAMIC) {
            INIT_LOGI("%s is dynamic service.", service->name);
            NotifyServiceChange(service->name, "stopped");
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        if (ServiceStart(service) != SERVICE_SUCCESS) {
            INIT_LOGE("Service %s start failed!", service->name);
        }
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
#endif
    INIT_LOGI("StartAllServices %d finsh", startMode);
}

void LoadAccessTokenId(void)
{
    GetAccessToken();
}
