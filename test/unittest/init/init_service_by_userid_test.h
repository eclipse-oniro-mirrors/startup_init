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

#ifndef INIT_SERVICE_BY_USERID_TEST_H
#define INIT_SERVICE_BY_USERID_TEST_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>

#include "init_service.h"
#include "init_service_by_userid.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef int (*ByUserStatFunc)(const char *pathname, struct stat *buf);
typedef int (*ByUserWriteParamFunc)(const char *name, const char *value);

typedef struct {
    char serviceName[64];
    char instanceKey[80];
    int32_t userId;
    int pid;
    int status;
    int lastErrno;
    uintptr_t templateService;
    char templateName[64];
    int templatePid;
    int templateStatus;
    unsigned int templateAttribute;
    char mcs[128];
} ServiceByUserIdTestSnapshot;

typedef struct {
    char serviceName[64];
    int32_t userId;
    int extArgc;
    char extArgs[4][96];
} ByUserCtrlArgsTestSnapshot;

typedef struct {
    int serviceStartCount;
    char serviceStartName[128];
    int64_t serviceStartPid;
    int64_t serviceStartMode;
    int childExitCount;
    char childExitName[128];
    int childExitPid;
    int childExitCode;
    int64_t childExitMode;
} ByUserReportTestSnapshot;

void TestResetServiceByUserIdInstances(void);
int TestGetServiceByUserIdCount(void);
char *TestSplitNextToken(char **cursor);
int TestCopyToken(char *dest, size_t size, const char *token);
int TestCopyExtToken(char *dest, size_t size, const char *token);
int TestHasByUserCtrlBlankChar(const char *value);
int TestDecodeUserId(const char *token, int32_t *userId);
int TestParseByUserCtrlValue(const char *value, ByUserCtrlArgsTestSnapshot *snapshot);
int TestBuildByUserStatusKey(char *buffer, size_t size, const char *serviceName, int32_t userId);
int TestBuildByUserPidKey(char *buffer, size_t size, const char *serviceName, int32_t userId);
int TestBuildInstanceKey(char *buffer, size_t size, const char *serviceName, int32_t userId);
int TestComputeMcsByUserId(int32_t userId, char *buffer, size_t size);
int TestBuildRuntimePathArgs(const Service *service, const char *encodedValue, ServiceArgs *runtimePathArgs);
int TestBuildRuntimePathArgsWithNullArgs(const Service *service, ServiceArgs *runtimePathArgs);
void TestFreeRuntimePathArgs(ServiceArgs *runtimePathArgs);
int TestInitRuntimeService(Service *runtimeService, const ServiceByUserId *instance,
    const ServiceArgs *pathArgs, int pid);
int TestIsByUserServiceInvalid(ServiceByUserId *instance, const Service *service, const ServiceArgs *pathArgs);
int TestValidateByUserTemplate(const Service *service);
ServiceByUserId *TestGetOrCreateServiceByUserId(const Service *templateService, const char *serviceName,
    int32_t userId);
int TestFindServiceByUserIdSnapshot(const char *serviceName, int32_t userId, ServiceByUserIdTestSnapshot *snapshot);
void TestSetServiceByUserIdState(const char *serviceName, int32_t userId, int pid, int status);
void TestSetServiceByUserIdTemplate(const char *serviceName, int32_t userId, const Service *templateService);
void TestSetServiceByUserIdUser(const char *serviceName, int32_t oldUserId, int32_t newUserId);
int TestWriteByUserServiceState(const char *serviceName, int32_t userId, int status);
int TestStartNullServiceInstanceByUserId(void);
int TestStopNullServiceInstanceArgsByUserId(void);
int TestWriteNullServiceByUserState(void);
int TestStartInstanceProcess(const char *encodedValue);
int TestStartInstanceProcessWithNullInstance(void);
int TestStartInstanceProcessWithNullArgs(ServiceByUserId *instance);
int TestStartInstanceProcessWithEmptyArgs(ServiceByUserId *instance);
int TestStartInstanceProcessWithPathArgs(const char *serviceName, int32_t userId, ServiceArgs *pathArgs);
int TestStartNullInstanceProcessWithPathArgs(ServiceArgs *pathArgs);
int SetUserIdToAccessToken(const Service *service, int32_t userId);
void TestReportServiceStartByUserInfo(Service *service, int pid);
void TestResetByUserReportSnapshot(void);
void TestGetByUserReportSnapshot(ByUserReportTestSnapshot *snapshot);
void TestRemoveMissingServiceByUserIdInstance(void);
void TestStartServiceByUserId(const char *encodedValue);
void TestStopServiceByUserId(const char *encodedValue);
int TestStopServiceInstanceByUserId(const char *encodedValue);
int TestStopNullServiceByUserIdInstance(void);
void TestFreeNullServiceByUserIdInstance(void);
void TestRemoveNullServiceByUserIdInstance(void);
void TestStopAllServiceByUserIdInstances(void);
ServiceByUserId *TestGetServiceByUserPid(pid_t pid);
int TestReapServiceByUserId(pid_t pid, int procStat);
void TestSetByUserStatFunc(ByUserStatFunc func);
void TestSetByUserWriteParamFunc(ByUserWriteParamFunc func);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
