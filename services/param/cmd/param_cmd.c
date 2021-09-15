/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "param_manager.h"
#include "param_utils.h"
#include "sys_param.h"

#define USAGE_INFO_PARAM_GET "param get [key]"
#define USAGE_INFO_PARAM_SET "param set key value"
#define USAGE_INFO_PARAM_WAIT "param wait key value"
#define USAGE_INFO_PARAM_DUMP "param dump [verbose]"
#define USAGE_INFO_PARAM_READ "param read key"
#define USAGE_INFO_PARAM_WATCH "param watch key"
#define READ_DURATION 100000
#define MIN_ARGC 2
#define WAIT_TIMEOUT_INDEX 2

struct CmdArgs {
    char name[8];
    int minArg;
    void (*DoFuncion)(int argc, char *argv[], int start);
    char help[128];
};

static void ShowParam(ParamHandle handle, void *cookie)
{
    char *name = (char *)cookie;
    char *value = ((char *)cookie) + PARAM_NAME_LEN_MAX;
    SystemGetParameterName(handle, name, PARAM_NAME_LEN_MAX);
    uint32_t size = PARAM_CONST_VALUE_LEN_MAX;
    SystemGetParameterValue(handle, value, &size);
    printf("\t%s = %s \n", name, value);
}

static void ExeuteCmdParamGet(int argc, char *argv[], int start)
{
    uint32_t size = PARAM_CONST_VALUE_LEN_MAX + PARAM_NAME_LEN_MAX + 1 + 1;
    char *buffer = (char *)malloc(size);
    if (buffer == NULL) {
        printf("Get parameterfail\n");
        return;
    }
    memset_s(buffer, size, 0, size);
    if (argc == start) {
        SystemTraversalParameter(ShowParam, (void *)buffer);
    } else {
        int ret = SystemGetParameter(argv[start], buffer, &size);
        if (ret == 0) {
            printf("%s \n", buffer);
        } else {
            printf("Get parameter \"%s\" fail\n", argv[start]);
        }
    }
    free(buffer);
}

static void ExeuteCmdParamSet(int argc, char *argv[], int start)
{
    UNUSED(argc);
    int ret = SystemSetParameter(argv[start], argv[start + 1]);
    if (ret == 0) {
        printf("Set parameter %s %s success\n", argv[start], argv[start + 1]);
    } else {
        printf("Set parameter %s %s fail\n", argv[start], argv[start + 1]);
    }
    return;
}

static void ExeuteCmdParamDump(int argc, char *argv[], int start)
{
    int verbose = 0;
    if (argc > start && strcmp(argv[start], "verbose") == 0) {
        verbose = 1;
    }
    SystemDumpParameters(verbose);
}

static void ExeuteCmdParamWait(int argc, char *argv[], int start)
{
    char *value = NULL;
    uint32_t timeout = DEFAULT_PARAM_WAIT_TIMEOUT;
    if (argc > (start + 1)) {
        value = argv[start + 1];
    }
    if (argc > (start + WAIT_TIMEOUT_INDEX)) {
        timeout = atol(argv[start + WAIT_TIMEOUT_INDEX]);
    }
    SystemWaitParameter(argv[start], value, timeout);
}

#ifdef PARAM_TEST
static void ExeuteCmdParamRead(int argc, char *argv[], int start)
{
    srand((unsigned)time(NULL)); // srand()函数产生一个以当前时间开始的随机种子
    while (1) {
        ExeuteCmdParamGet(argc, argv, start);
        int wait = rand() / READ_DURATION + READ_DURATION; // 100ms
        usleep(wait);
    }
}

static void HandleParamChange(const char *key, const char *value, void *context)
{
    printf("Receive parameter change %s %s \n", key, value);
}

static void ExeuteCmdParamWatch(int argc, char *argv[], int start)
{
    int ret = SystemWatchParameter(argv[start], HandleParamChange, NULL);
    if (ret != 0) {
        return;
    }
    while (1) {
        (void)pause();
    }
}
#endif

int RunParamCommand(int argc, char *argv[])
{
    static struct CmdArgs paramCmds[] = {
        { "set", 4, ExeuteCmdParamSet, USAGE_INFO_PARAM_SET },
        { "get", 2, ExeuteCmdParamGet, USAGE_INFO_PARAM_GET },
        { "wait", 3, ExeuteCmdParamWait, USAGE_INFO_PARAM_WAIT },
        { "dump", 2, ExeuteCmdParamDump, USAGE_INFO_PARAM_DUMP },
#ifdef PARAM_TEST
        { "read", 2, ExeuteCmdParamRead, USAGE_INFO_PARAM_READ },
        { "watch", 2, ExeuteCmdParamWatch, USAGE_INFO_PARAM_WATCH },
#endif
	};
    if (argc < MIN_ARGC) {
        printf("usage: \n");
        for (size_t i = 0; i < sizeof(paramCmds) / sizeof(paramCmds[0]); i++) {
            printf("\t %s\n", paramCmds[i].help);
        }
        return 0;
    }

    for (size_t i = 0; i < sizeof(paramCmds) / sizeof(paramCmds[0]); i++) {
        if (strncmp(argv[1], paramCmds[i].name, strlen(paramCmds[i].name)) == 0) {
            if (argc < paramCmds[i].minArg) {
                printf("usage: %s\n", paramCmds[i].help);
                return 0;
            }
            paramCmds[i].DoFuncion(argc, argv, MIN_ARGC);
            return 0;
        }
    }
    printf("usage: \n");
    for (size_t i = 0; i < sizeof(paramCmds) / sizeof(paramCmds[0]); i++) {
        printf("\t%s\n", paramCmds[i].help);
    }
    return 0;
}

#ifndef STARTUP_INIT_TEST
int main(int argc, char *argv[])
{
    return RunParamCommand(argc, argv);
}
#endif