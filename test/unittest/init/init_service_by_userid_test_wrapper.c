/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "init_service_by_userid_test.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "param/init_param.h"
#include "securec.h"

static ByUserStatFunc g_testByUserStatFunc = NULL;
static ByUserWriteParamFunc g_testByUserWriteParamFunc = NULL;
static ByUserReportTestSnapshot g_testByUserReportSnapshot = {0};

void TestReportServiceStart(char *serviceName, int64_t pid, int64_t spawnMode)
{
    g_testByUserReportSnapshot.serviceStartCount++;
    g_testByUserReportSnapshot.serviceStartPid = pid;
    g_testByUserReportSnapshot.serviceStartMode = spawnMode;
    if (serviceName != NULL) {
        (void)strcpy_s(g_testByUserReportSnapshot.serviceStartName,
            sizeof(g_testByUserReportSnapshot.serviceStartName), serviceName);
    }
}

void TestReportChildProcessExit(const char *serviceName, int pid, int err, int64_t spawnMode)
{
    g_testByUserReportSnapshot.childExitCount++;
    g_testByUserReportSnapshot.childExitPid = pid;
    g_testByUserReportSnapshot.childExitCode = err;
    g_testByUserReportSnapshot.childExitMode = spawnMode;
    if (serviceName != NULL) {
        (void)strcpy_s(g_testByUserReportSnapshot.childExitName,
            sizeof(g_testByUserReportSnapshot.childExitName), serviceName);
    }
}

static int TestByUserStat(const char *pathname, struct stat *buf)
{
    if (g_testByUserStatFunc != NULL) {
        return g_testByUserStatFunc(pathname, buf);
    }
    return stat(pathname, buf);
}

static int TestByUserWriteParam(const char *name, const char *value)
{
    if (g_testByUserWriteParamFunc != NULL) {
        return g_testByUserWriteParamFunc(name, value);
    }
    return SystemWriteParam(name, value);
}

#define BY_USER_STAT TestByUserStat
#define BY_USER_SYSTEM_WRITE_PARAM TestByUserWriteParam
#define ReportServiceStart TestReportServiceStart  // NOLINT
#define ReportChildProcessExit TestReportChildProcessExit  // NOLINT
#include "../../../services/init/init_service_by_userid.c"
#undef ReportChildProcessExit
#undef ReportServiceStart
#undef BY_USER_SYSTEM_WRITE_PARAM
#undef BY_USER_STAT

static void CopySnapshotString(char *dest, size_t size, const char *source)
{
    if (dest == NULL || size == 0 || source == NULL) {
        return;
    }
    int ret = strcpy_s(dest, size, source);
    if (ret != EOK) {
        dest[0] = '\0';
    }
}

static void FillServiceByUserIdSnapshot(const ServiceByUserId *instance, ServiceByUserIdTestSnapshot *snapshot)
{
    if (instance == NULL || snapshot == NULL) {
        return;
    }
    (void)memset_s(snapshot, sizeof(*snapshot), 0, sizeof(*snapshot));
    CopySnapshotString(snapshot->serviceName, sizeof(snapshot->serviceName), instance->serviceName);
    CopySnapshotString(snapshot->instanceKey, sizeof(snapshot->instanceKey), instance->instanceKey);
    snapshot->userId = instance->userId;
    snapshot->pid = instance->pid;
    snapshot->status = instance->status;
    snapshot->lastErrno = instance->lastErrno;
    snapshot->templateService = (uintptr_t)instance->templateService;
    if (instance->templateService != NULL) {
        CopySnapshotString(snapshot->templateName, sizeof(snapshot->templateName),
            instance->templateService->name == NULL ? "" : instance->templateService->name);
        snapshot->templatePid = instance->templateService->pid;
        snapshot->templateStatus = instance->templateService->status;
        snapshot->templateAttribute = instance->templateService->attribute;
    }
    CopySnapshotString(snapshot->mcs, sizeof(snapshot->mcs), instance->mcs);
}

void TestResetServiceByUserIdInstances(void)
{
    ServiceByUserId *instance = g_serviceByUserIdHead;
    while (instance != NULL) {
        ServiceByUserId *next = instance->next;
        FreeServiceByUserIdInstance(instance);
        instance = next;
    }
    g_serviceByUserIdHead = NULL;
}

int TestGetServiceByUserIdCount(void)
{
    int count = 0;
    ServiceByUserId *instance = g_serviceByUserIdHead;
    while (instance != NULL) {
        count++;
        instance = instance->next;
    }
    return count;
}

char *TestSplitNextToken(char **cursor)
{
    return SplitNextToken(cursor);
}

int TestCopyToken(char *dest, size_t size, const char *token)
{
    return CopyToken(dest, size, token);
}

int TestCopyExtToken(char *dest, size_t size, const char *token)
{
    return CopyExtToken(dest, size, token);
}

int TestHasByUserCtrlBlankChar(const char *value)
{
    return HasByUserCtrlBlankChar(value);
}

int TestDecodeUserId(const char *token, int32_t *userId)
{
    return DecodeUserId(token, userId);
}

int TestParseByUserCtrlValue(const char *value, ByUserCtrlArgsTestSnapshot *snapshot)
{
    ByUserCtrlArgs args = {0};
    int ret = ParseByUserCtrlValue(value, &args);
    if (ret != 0 || snapshot == NULL) {
        return ret;
    }
    (void)memset_s(snapshot, sizeof(*snapshot), 0, sizeof(*snapshot));
    CopySnapshotString(snapshot->serviceName, sizeof(snapshot->serviceName), args.serviceName);
    snapshot->userId = args.userId;
    snapshot->extArgc = args.extArgc;
    for (int i = 0; i < args.extArgc && i < BY_USER_EXT_ARG_MAX; i++) {
        CopySnapshotString(snapshot->extArgs[i], sizeof(snapshot->extArgs[i]), args.extArgs[i]);
    }
    return 0;
}

int TestBuildByUserStatusKey(char *buffer, size_t size, const char *serviceName, int32_t userId)
{
    return BuildByUserStatusKey(buffer, size, serviceName, userId);
}

int TestBuildByUserPidKey(char *buffer, size_t size, const char *serviceName, int32_t userId)
{
    return BuildByUserPidKey(buffer, size, serviceName, userId);
}

int TestBuildInstanceKey(char *buffer, size_t size, const char *serviceName, int32_t userId)
{
    return BuildInstanceKey(buffer, size, serviceName, userId);
}

int TestComputeMcsByUserId(int32_t userId, char *buffer, size_t size)
{
    return ComputeMcsByUserId(userId, buffer, size);
}

int TestBuildRuntimePathArgs(const Service *service, const char *encodedValue, ServiceArgs *runtimePathArgs)
{
    ByUserCtrlArgs args = {0};
    if (ParseByUserCtrlValue(encodedValue, &args) != 0) {
        return -1;
    }
    return BuildRuntimePathArgs(service, &args, runtimePathArgs) == NULL ? -1 : 0;
}

int TestBuildRuntimePathArgsWithNullArgs(const Service *service, ServiceArgs *runtimePathArgs)
{
    return BuildRuntimePathArgs(service, NULL, runtimePathArgs) == NULL ? -1 : 0;
}

void TestFreeRuntimePathArgs(ServiceArgs *runtimePathArgs)
{
    FreeRuntimePathArgs(runtimePathArgs);
}

int TestInitRuntimeService(Service *runtimeService, const ServiceByUserId *instance,
    const ServiceArgs *pathArgs, int pid)
{
    return InitRuntimeService(runtimeService, instance, pathArgs, pid);
}

int TestIsByUserServiceInvalid(ServiceByUserId *instance, const Service *service, const ServiceArgs *pathArgs)
{
    return IsByUserServiceInvalid(instance, service, pathArgs);
}

int TestValidateByUserTemplate(const Service *service)
{
    return ValidateByUserTemplate(service);
}

ServiceByUserId *TestGetOrCreateServiceByUserId(const Service *templateService, const char *serviceName,
    int32_t userId)
{
    return GetOrCreateServiceByUserId(templateService, serviceName, userId);
}

int TestFindServiceByUserIdSnapshot(const char *serviceName, int32_t userId, ServiceByUserIdTestSnapshot *snapshot)
{
    ServiceByUserId *instance = FindServiceByUserId(serviceName, userId);
    if (instance == NULL) {
        return -1;
    }
    FillServiceByUserIdSnapshot(instance, snapshot);
    return 0;
}

void TestSetServiceByUserIdState(const char *serviceName, int32_t userId, int pid, int status)
{
    ServiceByUserId *instance = FindServiceByUserId(serviceName, userId);
    if (instance == NULL) {
        return;
    }
    instance->pid = pid;
    instance->status = status;
}

void TestSetServiceByUserIdTemplate(const char *serviceName, int32_t userId, const Service *templateService)
{
    ServiceByUserId *instance = FindServiceByUserId(serviceName, userId);
    if (instance == NULL) {
        return;
    }
    instance->templateService = templateService;
}

void TestSetServiceByUserIdUser(const char *serviceName, int32_t oldUserId, int32_t newUserId)
{
    ServiceByUserId *instance = FindServiceByUserId(serviceName, oldUserId);
    if (instance == NULL) {
        return;
    }
    instance->userId = newUserId;
}

void TestSetByUserStatFunc(ByUserStatFunc func)
{
    g_testByUserStatFunc = func;
}

void TestSetByUserWriteParamFunc(ByUserWriteParamFunc func)
{
    g_testByUserWriteParamFunc = func;
}

int TestWriteByUserServiceState(const char *serviceName, int32_t userId, int status)
{
    ServiceByUserId *instance = FindServiceByUserId(serviceName, userId);
    if (instance == NULL) {
        return -1;
    }
    return WriteByUserServiceState(instance, status);
}

int TestStartNullServiceInstanceByUserId(void)
{
    return StartServiceInstanceByUserId(NULL);
}

int TestStopNullServiceInstanceArgsByUserId(void)
{
    return StopServiceInstanceByUserId(NULL);
}

int TestWriteNullServiceByUserState(void)
{
    return WriteByUserServiceState(NULL, SERVICE_STOPPED);
}

void TestReportServiceStartByUserInfo(Service *service, int pid)
{
    ReportByUserServiceStartInfo(service, pid);
}

void TestResetByUserReportSnapshot(void)
{
    (void)memset_s(&g_testByUserReportSnapshot, sizeof(g_testByUserReportSnapshot),
        0, sizeof(g_testByUserReportSnapshot));
}

void TestGetByUserReportSnapshot(ByUserReportTestSnapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    *snapshot = g_testByUserReportSnapshot;
}

void TestRemoveMissingServiceByUserIdInstance(void)
{
    ServiceByUserId missing = {0};
    RemoveServiceByUserIdInstance(&missing);
}

int TestStartInstanceProcess(const char *encodedValue)
{
    ByUserCtrlArgs args = {0};
    if (ParseByUserCtrlValue(encodedValue, &args) != 0) {
        return -1;
    }
    return StartServiceInstanceByUserId(&args);
}

int TestStartInstanceProcessWithNullInstance(void)
{
    ByUserCtrlArgs args = {0};
    return StartInstanceProcess(NULL, &args);
}

int TestStartInstanceProcessWithNullArgs(ServiceByUserId *instance)
{
    return StartInstanceProcess(instance, NULL);
}

int TestStartInstanceProcessWithEmptyArgs(ServiceByUserId *instance)
{
    ByUserCtrlArgs args = {0};
    return StartInstanceProcess(instance, &args);
}

int TestStartInstanceProcessWithPathArgs(const char *serviceName, int32_t userId, ServiceArgs *pathArgs)
{
    ServiceByUserId *instance = FindServiceByUserId(serviceName, userId);
    if (instance == NULL) {
        return -1;
    }
    return StartInstanceProcessWithPathArgs(instance, pathArgs);
}

int TestStartNullInstanceProcessWithPathArgs(ServiceArgs *pathArgs)
{
    return StartInstanceProcessWithPathArgs(NULL, pathArgs);
}

void TestStartServiceByUserId(const char *encodedValue)
{
    StartServiceByUserId(encodedValue);
}

void TestStopServiceByUserId(const char *encodedValue)
{
    StopServiceByUserId(encodedValue);
}

int TestStopServiceInstanceByUserId(const char *encodedValue)
{
    ByUserCtrlArgs args = {0};
    if (ParseByUserCtrlValue(encodedValue, &args) != 0) {
        return -1;
    }
    return StopServiceInstanceByUserId(&args);
}

int TestStopNullServiceByUserIdInstance(void)
{
    return StopInstanceProcess(NULL, SIGTERM);
}

void TestFreeNullServiceByUserIdInstance(void)
{
    FreeServiceByUserIdInstance(NULL);
}

void TestRemoveNullServiceByUserIdInstance(void)
{
    RemoveServiceByUserIdInstance(NULL);
}

void TestStopAllServiceByUserIdInstances(void)
{
    StopAllServiceByUserIdInstances();
}

ServiceByUserId *TestGetServiceByUserPid(pid_t pid)
{
    return GetServiceByUserPid(pid);
}

int TestReapServiceByUserId(pid_t pid, int procStat)
{
    return ReapServiceByUserId(pid, procStat);
}
