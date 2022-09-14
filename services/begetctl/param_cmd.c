/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <grp.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>

#include "begetctl.h"
#include "param_manager.h"
#include "param_security.h"
#include "shell_utils.h"
#include "init_param.h"
#include "beget_ext.h"
#ifdef PARAM_SUPPORT_SELINUX
#include <policycoreutils.h>
#include <selinux/selinux.h>
#include "selinux_parameter.h"
#endif // PARAM_SUPPORT_SELINUX

#define MASK_LENGTH_MAX 4
pid_t g_shellPid = 0;
static struct termios g_terminalState;
char g_isSetTerminal = 0;

void demoExit(void)
{
    BShellEnvDestory(GetShellHandle());
    if (g_shellPid != 0) {
#ifndef STARTUP_INIT_TEST
        kill(g_shellPid, SIGKILL);
#endif
    }
    BEGET_CHECK(g_isSetTerminal == 0, tcsetattr(0, TCSAFLUSH, &g_terminalState));
}

#define CMD_PATH "/system/bin/paramshell"
#define SHELL_NAME "paramshell"
#ifndef TIOCSCTTY
#define TIOCSCTTY 0x540E
#endif
static char *GetLocalBuffer(uint32_t *buffSize)
{
    static char buffer[PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX] = {0};
    BEGET_CHECK(buffSize == NULL, *buffSize = sizeof(buffer));
    return buffer;
}

static char *GetRealParameter(BShellHandle shell, const char *name, char *buffer, uint32_t buffSize)
{
    BSH_CHECK(buffer != NULL && name != NULL, return NULL, "Invalid parameter");
    const BShellParam *param = BShellEnvGetParam(shell, PARAM_REVERESD_NAME_CURR_PARAMETER);
    const char *current = (param == NULL) ? "" : param->value.string;
    int32_t realLen = 0;
    int ret = 0;
    if (name[0] == '.') { // relatively
        if (strcmp(name, "..") == 0) {
            char *tmp = strrchr(current, '.');
            if (tmp != NULL) {
                realLen = tmp - current;
                ret = memcpy_s(buffer, buffSize, current, realLen);
            } else {
                ret = memcpy_s(buffer, buffSize, "#", 1);
                realLen = 1;
            }
            BSH_CHECK(ret == 0, return NULL, "Failed to memcpy");
        } else if (strcmp(name, ".") == 0) {
            realLen = sprintf_s(buffer, buffSize, "%s", current);
        } else {
            realLen = sprintf_s(buffer, buffSize, "%s%s", current, name);
        }
    } else if (strlen(name) == 0) {
        realLen = sprintf_s(buffer, buffSize, "%s", current);
    } else {
        realLen = sprintf_s(buffer, buffSize, "%s", name);
    }
    BSH_CHECK(realLen >= 0, return NULL, "Failed to format buffer");
    buffer[realLen] = '\0';
    BSH_LOGV("GetRealParameter current %s input %s real %s", current, name, buffer);
    return buffer;
}

int SetParamShellPrompt(BShellHandle shell, const char *param)
{
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    char *realParameter = GetRealParameter(shell, param, buffer, buffSize);
    BSH_CHECK(realParameter != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    if (strlen(realParameter) == 0) {
        BShellEnvOutputPrompt(shell, PARAM_SHELL_DEFAULT_PROMPT);
        return -1;
    }
    // check parameter
    int ret = SystemCheckParamExist(realParameter);
    if (ret == PARAM_CODE_NOT_FOUND) {
        BShellEnvOutput(shell, "Error: parameter \'%s\' not found\r\n", realParameter);
        return -1;
    } else if (ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        BShellEnvOutput(shell, "Error: Forbid to enter parameters \'%s\'\r\n", realParameter);
        return -1;
    }
    if (strcmp(realParameter, "#") == 0) {
        ret = BShellEnvSetParam(shell, PARAM_REVERESD_NAME_CURR_PARAMETER,
            "", PARAM_STRING, (void *)"");
        BSH_CHECK(ret == 0, return BSH_SYSTEM_ERR, "Failed to set param value");
        BShellEnvOutputPrompt(shell, PARAM_SHELL_DEFAULT_PROMPT);
        return 0;
    }
    ret = BShellEnvSetParam(shell, PARAM_REVERESD_NAME_CURR_PARAMETER,
        "", PARAM_STRING, (void *)realParameter);
    BSH_CHECK(ret == 0, return BSH_SYSTEM_ERR, "Failed to set param value");
    if (strcat_s(realParameter, buffSize, "#") != 0) {
        BSH_CHECK(ret != 0, return BSH_SYSTEM_ERR, "Failed to cat prompt %s", realParameter);
    }
    BShellEnvOutputPrompt(shell, realParameter);
    return 0;
}

static char *GetPermissionString(uint32_t mode, int shift, char *str, int size)
{
    BEGET_CHECK(!(size < MASK_LENGTH_MAX), return str);
    str[0] = '-';
    str[1] = '-';
    str[2] = '-'; // 2 watcher
    str[3] = '\0'; // 3 end
    if (mode & (DAC_READ >> shift)) {
        str[0] = 'r';
    }
    if (mode & (DAC_WRITE >> shift)) {
        str[1] = 'w';
    }
    if (mode & (DAC_WATCH >> shift)) {
        str[2] = 'w'; // 2 watcher
    }
    return str;
}

static void ShowParam(BShellHandle shell, const char *name, const char *value)
{
    ParamAuditData auditData = {};
    int ret = GetParamSecurityAuditData(name, 0, &auditData);
    BSH_CHECK(ret == 0, return, "Failed to get param security for %s", name);
    BShellEnvOutput(shell, "Parameter information:\r\n");
#ifdef PARAM_SUPPORT_SELINUX
    BShellEnvOutput(shell, "selinux  : %s \r\n", auditData.label);
#endif
    char permissionStr[3][MASK_LENGTH_MAX] = {}; // 3 permission
    struct passwd *user = getpwuid(auditData.dacData.uid);
    struct group *group = getgrgid(auditData.dacData.gid);
    if (user != NULL && group != NULL) {
        BShellEnvOutput(shell, "    dac  : %s(%s) %s(%s) (%s) \r\n",
            user->pw_name,
            GetPermissionString(auditData.dacData.mode, 0, permissionStr[0], MASK_LENGTH_MAX),
            group->gr_name,
            GetPermissionString(auditData.dacData.mode,DAC_GROUP_START, permissionStr[1], MASK_LENGTH_MAX),
             // 2 other
            GetPermissionString(auditData.dacData.mode, DAC_OTHER_START, permissionStr[2], MASK_LENGTH_MAX));
    }
    if (strcmp("#", name) != 0) {
        BShellEnvOutput(shell, "    name : %s\r\n", name);
    }
    if (value != NULL) {
        BShellEnvOutput(shell, "    value: %s\r\n", value);
    }
}

static void ShowParamForCmdLs(ParamHandle handle, void *cookie)
{
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    if (buffSize < (PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX)) {
        return;
    }
    char *value = buffer + PARAM_NAME_LEN_MAX;
    (void)SystemGetParameterName(handle, buffer, PARAM_NAME_LEN_MAX);
    uint32_t valueLen = buffSize - PARAM_NAME_LEN_MAX;
    (void)SystemGetParameterValue(handle, value, &valueLen);
    ShowParam((BShellHandle)cookie, buffer, value);
}

static int32_t BShellParamCmdLs(BShellHandle shell, int32_t argc, char *argv[])
{
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    int all = 0;
    char *input = NULL;
    for (int32_t i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            all = 1;
        } else if (input == NULL) {
            input = argv[i];
        }
    }
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    char *realParameter = GetRealParameter(shell, (input == NULL) ? "" : input, buffer, buffSize);
    BSH_CHECK(realParameter != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    char *prefix = strdup((strlen(realParameter) == 0) ? "#" : realParameter);
    BSH_LOGV("BShellParamCmdLs prefix %s", prefix);
    int ret = 0;
    if (all != 0) {
        ret = SystemTraversalParameter(prefix, ShowParamForCmdLs, (void *)shell);
        if (ret != 0) {
            BShellEnvOutput(shell, "Error: Forbid to list parameters\r\n");
        }
    } else {
        ret = SystemCheckParamExist(prefix);
        if (ret == 0) {
            ParamHandle handle;
            ret = SystemFindParameter(prefix, &handle);
            if (ret != 0) {
                BShellEnvOutput(shell, "Error: Forbid to list parameters\r\n");
            } else {
                ShowParamForCmdLs(handle, (void *)shell);
            }
        } else if (ret == PARAM_CODE_NODE_EXIST) {
            ShowParam(shell, prefix, NULL);
        } else if (ret != PARAM_CODE_NOT_FOUND) {
            BShellEnvOutput(shell, "Error: Forbid to list parameters\r\n");
        } else {
            BShellEnvOutput(shell, "Parameter %s not found\r\n", prefix);
        }
    }
    free(prefix);
    return 0;
}

static int32_t BShellParamCmdCat(BShellHandle shell, int32_t argc, char *argv[])
{
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_CHECK(argc >= 1, return BSH_CMD_PARAM_INVALID, "Invalid shell env");
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    char *realParameter = GetRealParameter(shell, argv[1], buffer, buffSize);
    BSH_CHECK(realParameter != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    int ret = SystemGetParameter(realParameter, buffer, &buffSize);
    BSH_CHECK(ret != 0, BShellEnvOutput(shell, "    %s\r\n", buffer));
    return 0;
}

static int32_t BShellParamCmdCd(BShellHandle shell, int32_t argc, char *argv[])
{
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_CHECK(argc >= 1, return BSH_CMD_PARAM_INVALID, "Invalid shell env");
    SetParamShellPrompt(shell, argv[1]);
    return 0;
}

static void ShowParamForCmdGet(ParamHandle handle, void *cookie)
{
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    BSH_CHECK(!(buffSize < (PARAM_NAME_LEN_MAX + PARAM_CONST_VALUE_LEN_MAX)), return);
    char *value = buffer + PARAM_NAME_LEN_MAX;
    (void)SystemGetParameterName(handle, buffer, PARAM_NAME_LEN_MAX);
    uint32_t valueLen = buffSize - PARAM_NAME_LEN_MAX;
    (void)SystemGetParameterValue(handle, value, &valueLen);
    BShellEnvOutput((BShellHandle)cookie, "    %s = %s\r\n", buffer, value);
}

static int32_t BShellParamCmdGet(BShellHandle shell, int32_t argc, char *argv[])
{
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_CHECK(argc >= 1, return BSH_CMD_PARAM_INVALID, "Invalid shell env");
    int ret = 0;
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    char *realParameter = GetRealParameter(shell, (argc == 1) ? "" : argv[1], buffer, buffSize);
    if ((argc == 1) || (realParameter == NULL) ||
        (strlen(realParameter) == 0) || (strcmp(realParameter, "#") == 0)) {
        ret = SystemTraversalParameter(realParameter, ShowParamForCmdGet, (void *)shell);
        if (ret != 0) {
            BShellEnvOutput(shell, "Error: Forbid to get all parameters\r\n");
        }
        return 0;
    }
    char *key = strdup(realParameter);
    ret = SystemGetParameter(key, buffer, &buffSize);
    if (ret == 0) {
        BShellEnvOutput(shell, "%s \n", buffer);
    } else {
        BShellEnvOutput(shell, "Get parameter \"%s\" fail\n", key);
    }
    free(key);
    return 0;
}

static int32_t BShellParamCmdSet(BShellHandle shell, int32_t argc, char *argv[])
{
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    if (argc < 3) { // 3 min param
        char *helpArgs[] = {"param", NULL};
        BShellCmdHelp(shell, 1, helpArgs);
        return 0;
    }
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    char *realParameter = GetRealParameter(shell, argv[1], buffer, buffSize);
    if ((realParameter == NULL) || (strlen(realParameter) == 0) || (strcmp(realParameter, "#") == 0)) {
        BShellEnvOutput(shell, "Set parameter %s %s fail\n", argv[1], argv[2]); // 2 value param
        return 0;
    }
    int ret = SystemSetParameter(realParameter, argv[2]); // 2 value param
    if (ret == 0) {
        BShellEnvOutput(shell, "Set parameter %s %s success\n", realParameter, argv[2]); // 2 value param
    } else {
        BShellEnvOutput(shell, "Set parameter %s %s fail\n", realParameter, argv[2]); // 2 value param
    }
    return 0;
}

static int32_t BShellParamCmdWait(BShellHandle shell, int32_t argc, char *argv[])
{
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    if (argc < 2) { // 2 min param
        char *helpArgs[] = {"param", NULL};
        BShellCmdHelp(shell, 1, helpArgs);
        return 0;
    }
    int32_t timeout = 30; // 30s
    char *value = "*";
    if (argc > 2) { // 2 value param
        value = argv[2]; // 2 value param
    }
    if (argc > 3) { // 3 timeout param
        timeout = atoi(argv[3]); // 3 timeout param
    }
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    char *realParameter = GetRealParameter(shell, argv[1], buffer, buffSize);
    if ((realParameter == NULL) || (strlen(realParameter) == 0) || (strcmp(realParameter, "#") == 0)) {
        BShellEnvOutput(shell, "Wait parameter %s fail\n", argv[1]);
        return 0;
    }

    int ret = SystemWaitParameter(realParameter, value, timeout);
    if (ret == 0) {
        BShellEnvOutput(shell, "Wait parameter %s success\n", argv[1]);
    } else {
        BShellEnvOutput(shell, "Wait parameter %s fail\n", argv[1]);
    }
    return 0;
}

static int32_t BShellParamCmdDump(BShellHandle shell, int32_t argc, char *argv[])
{
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    if (argc >= 2 && strcmp(argv[1], "verbose") == 0) { // 2 min arg
        SystemDumpParameters(1, printf);
    } else {
        SystemDumpParameters(0, printf);
    }
    return 0;
}

static int32_t BShellParamCmdPwd(BShellHandle shell, int32_t argc, char *argv[])
{
    uint32_t buffSize = 0;
    char *buffer = GetLocalBuffer(&buffSize);
    char *realParameter = GetRealParameter(shell, "", buffer, buffSize);
    BShellEnvOutput(shell, "%s\r\n", realParameter);
    return 0;
}

static int32_t BShellParamCmdShell(BShellHandle shell, int32_t argc, char *argv[])
{
#ifndef STARTUP_INIT_TEST
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_LOGV("BShellParamCmdShell %d %s", argc, argv[1]);
    int ret = 0;
    if (argc > 1) {
        ret = SystemCheckParamExist(argv[1]);
        if (ret != 0) {
            BShellEnvOutput(shell, "Error: parameter \'%s\' not found\r\n", argv[1]);
            return -1;
        }
    }
    if (tcgetattr(0, &g_terminalState)) {
        return BSH_SYSTEM_ERR;
    }
    g_isSetTerminal = 1;
    pid_t pid = fork();
    if (pid == 0) {
        setuid(2000); // 2000 shell group
        setgid(2000); // 2000 shell group
#ifdef PARAM_SUPPORT_SELINUX
        setcon("u:r:normal_hap_domain:s0");
#endif
        if (argc >= 2) { // 2 min argc
            char *args[] = {SHELL_NAME, argv[1], NULL};
            ret = execv(CMD_PATH, args);
        } else {
            char *args[] = {SHELL_NAME, NULL};
            ret = execv(CMD_PATH, args);
        }
        if (ret < 0) {
            printf("error on exec %d \n", errno);
            exit(0);
        }
        exit(0);
    } else if (pid > 0) {
        g_shellPid = pid;
        int status = 0;
        wait(&status);
        tcsetattr(0, TCSAFLUSH, &g_terminalState);
        g_isSetTerminal = 0;
    }
#endif
    return 0;
}

static int32_t BShellParamCmdRegForShell(BShellHandle shell)
{
    const CmdInfo infos[] = {
        {"ls", BShellParamCmdLs, "display system parameter", "ls [-r] [name]", NULL},
        {"get", BShellParamCmdGet, "get system parameter", "get [name]", NULL},
        {"set", BShellParamCmdSet, "set system parameter", "set name value", NULL},
        {"wait", BShellParamCmdWait, "wait system parameter", "wait name [value] [timeout]", NULL},
        {"dump", BShellParamCmdDump, "dump system parameter", "dump [verbose]", ""},
        {"cd", BShellParamCmdCd, "change path of parameter", "cd name", NULL},
        {"cat", BShellParamCmdCat, "display value of parameter", "cat name", NULL},
        {"pwd", BShellParamCmdPwd, "display current parameter", "pwd", NULL},
    };
    for (size_t i = sizeof(infos) / sizeof(infos[0]); i > 0; i--) {
        BShellEnvRegisterCmd(shell, &infos[i - 1]);
    }
    return 0;
}

static int32_t BShellParamCmdRegForIndepent(BShellHandle shell)
{
    const CmdInfo infos[] = {
        {"param", BShellParamCmdLs, "display system parameter", "param ls [-r] [name]", "param ls"},
        {"param", BShellParamCmdGet, "get system parameter", "param get [name]", "param get"},
        {"param", BShellParamCmdSet, "set system parameter", "param set name value", "param set"},
        {"param", BShellParamCmdWait, "wait system parameter", "param wait name [value] [timeout]", "param wait"},
        {"param", BShellParamCmdDump, "dump system parameter", "param dump [verbose]", "param dump"},
        {"param", BShellParamCmdShell, "shell system parameter", "param shell [name]", "param shell"},
    };
    for (size_t i = sizeof(infos) / sizeof(infos[0]); i > 0; i--) {
        BShellEnvRegisterCmd(shell, &infos[i - 1]);
    }
    return 0;
}

int32_t BShellParamCmdRegister(BShellHandle shell, int execMode)
{
    if (execMode) {
        BShellParamCmdRegForShell(shell);
    } else {
        BShellParamCmdRegForIndepent(shell);
    }
    return 0;
}
