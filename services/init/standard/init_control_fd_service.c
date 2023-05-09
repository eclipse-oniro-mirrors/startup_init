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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "control_fd.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_modulemgr.h"
#include "init_utils.h"
#include "init_log.h"
#include "init_group_manager.h"
#include "init_param.h"
#include "hookmgr.h"
#include "bootstage.h"

static void DumpServiceArgs(const char *info, const ServiceArgs *args)
{
    printf("\tservice %s count %d \n", info, args->count);
    for (int j = 0; j < args->count; j++) {
        if (args->argv[j] != NULL) {
            printf("\t\tinfo [%d] %s \n", j, args->argv[j]);
        }
    }
}

static void DumpServiceJobs(const Service *service)
{
    printf("\tservice job info \n");
    if (service->serviceJobs.jobsName[JOB_ON_BOOT] != NULL) {
        printf("\t\tservice boot job %s \n", service->serviceJobs.jobsName[JOB_ON_BOOT]);
    }
    if (service->serviceJobs.jobsName[JOB_ON_START] != NULL) {
        printf("\t\tservice start job %s \n", service->serviceJobs.jobsName[JOB_ON_START]);
    }
    if (service->serviceJobs.jobsName[JOB_ON_STOP] != NULL) {
        printf("\t\tservice stop job %s \n", service->serviceJobs.jobsName[JOB_ON_STOP]);
    }
    if (service->serviceJobs.jobsName[JOB_ON_RESTART] != NULL) {
        printf("\t\tservice restart job %s \n", service->serviceJobs.jobsName[JOB_ON_RESTART]);
    }
}

static void DumpServiceSocket(const Service *service)
{
    printf("\tservice socket info \n");
    ServiceSocket *sockopt = service->socketCfg;
    while (sockopt != NULL) {
        printf("\t\tsocket name: %s \n", sockopt->name);
        printf("\t\tsocket type: %u \n", sockopt->type);
        printf("\t\tsocket uid: %u \n", sockopt->uid);
        printf("\t\tsocket gid: %u \n", sockopt->gid);
        sockopt = sockopt->next;
    }
}

void DumpServiceHookExecute(const char *name, const char *info)
{
    SERVICE_INFO_CTX context;
    context.serviceName = name;
    context.reserved = info;
    (void)HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_DUMP, (void *)(&context), NULL);
}

static void DumpOneService(const Service *service)
{
    const InitArgInfo startModeMap[] = {
        {"condition", START_MODE_CONDITION},
        {"boot", START_MODE_BOOT},
        {"normal", START_MODE_NORMAL}
    };

    const static char *serviceStatusMap[] = {
        "created", "starting", "running", "ready",
        "stopping", "stopped", "suspended", "freezed", "disabled", "critical"
    };

    printf("\tservice name: [%s] \n", service->name);
    printf("\tservice pid: [%d] \n", service->pid);
    printf("\tservice crashCnt: [%d] \n", service->crashCnt);
    printf("\tservice attribute: [%u] \n", service->attribute);
    printf("\tservice importance: [%d] \n", service->importance);
    printf("\tservice startMode: [%s] \n", startModeMap[service->startMode].name);
    printf("\tservice status: [%s] \n", serviceStatusMap[service->status]);
    printf("\tservice perms uID [%u] \n", service->servPerm.uID);
    DumpServiceArgs("path arg", &service->pathArgs);
    DumpServiceArgs("writepid file", &service->writePidArgs);
    DumpServiceJobs(service);
    DumpServiceSocket(service);

    printf("\tservice perms groupId %d \n", service->servPerm.gIDCnt);
    for (int i = 0; i < service->servPerm.gIDCnt; i++) {
        printf("\t\tservice perms groupId %u \n", service->servPerm.gIDArray[i]);
    }
    printf("\tservice perms capability %u \n", service->servPerm.capsCnt);
    for (int i = 0; i < (int)service->servPerm.capsCnt; i++) {
        printf("\t\tservice perms capability %u \n", service->servPerm.caps[i]);
    }

    DumpServiceHookExecute(service->name, NULL);
}

static void PrintBootEventHead(const char *cmd)
{
    if (strcmp(cmd, "bootevent") == 0) {
        printf("\t%-20.20s\t%-50s\t%-20.20s\t%-20.20s\n",
            "service-name", "bootevent-name", "fork", "ready");
    }
    return;
}

static void DumpAllExtData(const char *cmd)
{
    PrintBootEventHead(cmd);
    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        if (node->data.service == NULL) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        DumpServiceHookExecute(node->name, cmd);
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
}

static void DumpAllServices(void)
{
    printf("Ready to dump all services: \n");
    InitGroupNode *node = GetNextGroupNode(NODE_TYPE_SERVICES, NULL);
    while (node != NULL) {
        if (node->data.service == NULL) {
            node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
            continue;
        }
        Service *service = node->data.service;
        DumpOneService(service);
        node = GetNextGroupNode(NODE_TYPE_SERVICES, node);
    }
    printf("Dump all services finished \n");
}

static void ProcessSandboxControlFd(uint16_t type, const char *serviceCmd)
{
    if ((type != ACTION_SANDBOX) || (serviceCmd == NULL)) {
        INIT_LOGE("Invalid parameter");
        return;
    }
    Service *service  = GetServiceByName(serviceCmd);
    if (service == NULL) {
        INIT_LOGE("Failed get service %s", serviceCmd);
        return;
    }
    EnterServiceSandbox(service);
    return;
}

static void ProcessDumpServiceControlFd(uint16_t type, const char *serviceCmd)
{
    if ((type != ACTION_DUMP) || (serviceCmd == NULL)) {
        return;
    }
    char *cmd = strrchr(serviceCmd, '#');
    if (cmd != NULL) {
        cmd[0] = '\0';
        cmd++;
    }

    if (strcmp(serviceCmd, "all") == 0) {
        if (cmd != NULL) {
            DumpAllExtData(cmd);
        } else {
            DumpAllServices();
        }
        return;
    }
    if (strcmp(serviceCmd, "parameter_service") == 0) {
        if (cmd != NULL && strcmp(cmd, "trigger") == 0) {
            SystemDumpTriggers(0, printf);
        }
        return;
    }
    Service *service  = GetServiceByName(serviceCmd);
    if (service != NULL) {
        if (cmd != NULL) {
            PrintBootEventHead(cmd);
            DumpServiceHookExecute(serviceCmd, cmd);
        } else {
            DumpOneService(service);
        }
    }
    return;
}

static void ProcessModuleMgrControlFd(uint16_t type, const char *serviceCmd)
{
    if ((type != ACTION_MODULEMGR) || (serviceCmd == NULL)) {
        return;
    }
    INIT_LOGE("ProcessModuleMgrControlFd argc [%s] \n", serviceCmd);
    if (strcmp(serviceCmd, "list") == 0) {
        InitModuleMgrDump();
        return;
    }
}

void ProcessControlFd(uint16_t type, const char *serviceCmd, const void *context)
{
    if ((type >= ACTION_MAX) || (serviceCmd == NULL)) {
        return;
    }
    switch (type) {
        case ACTION_SANDBOX :
            ProcessSandboxControlFd(type, serviceCmd);
            break;
        case ACTION_DUMP :
            ProcessDumpServiceControlFd(type, serviceCmd);
            break;
        case ACTION_MODULEMGR :
            ProcessModuleMgrControlFd(type, serviceCmd);
            break;
        default :
            INIT_LOGW("Unknown control fd type.");
            break;
    }
}

void InitControlFd(void)
{
    CmdServiceInit(INIT_CONTROL_FD_SOCKET_PATH, ProcessControlFd);
    return;
}
