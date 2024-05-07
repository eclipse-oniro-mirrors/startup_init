/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <getopt.h>
#include <iostream>
#include <map>
#include <securec.h>
#include <string>
#include <unistd.h>
#include <vector>

#ifdef ENABLE_ENTER_APPSPAWN_SANDBOX
#include "appspawn.h"
#endif
#include "begetctl.h"
#include "control_fd.h"
#include "init_utils.h"
#include "sandbox.h"
#include "sandbox_namespace.h"
#include "string_ex.h"

using namespace OHOS;
struct option g_options[] = {
    { "service_name", required_argument, nullptr, 's' },
    { "namespace_name", required_argument, nullptr, 'n' },
    { "process_name", required_argument, nullptr, 'p' },
    { "process_pid", required_argument, nullptr, 'b' },
    { "help", no_argument, nullptr, 'h' },
    { nullptr, 0, nullptr, 0 },
};

static void Usage()
{
    std::cout << "sandbox -s | -n [-p] | -p | -b | -h" << std::endl;
    std::cout << "sandbox -s, --service_name=sandbox service \"enter service sandbox\"" << std::endl;
    std::cout << "sandbox -n, --namespace_name=namespace name \"namespace name, system, chipset etc.\"" << std::endl;
    std::cout << "sandbox -p, --process=process name \"sh, hdcd, hdf_devhost, etc.\"" << std::endl;
    std::cout << "sandbox -b, --process_pid=process pid \"sh, enter native app sandbox, etc.\"" << std::endl;
    std::cout << "sandbox -h, --help \"Show help\"" << std::endl;
#ifndef STARTUP_INIT_TEST
    exit(0);
#endif
}

static void RunSandbox(const std::string &sandboxName)
{
    InitDefaultNamespace();
    if (!InitSandboxWithName(sandboxName.c_str())) {
        std::cout << "Init sandbox failed." << std::endl;
        return;
    }

    DumpSandboxByName(sandboxName.c_str());
    if (PrepareSandbox(sandboxName.c_str()) != 0) {
        std::cout << "Prepare sandbox failed." << std::endl;
        return;
    }
    EnterDefaultNamespace();
    CloseDefaultNamespace();
    EnterSandbox(sandboxName.c_str());
    return;
}

static void EnterShell()
{
    char *argv[] = { const_cast<char *>("sh"), nullptr };
    char *envp[] = { nullptr };
    if (execve("/system/bin/sh", argv, envp) != 0) {
        std::cout << "execve sh failed! err = "<< errno << std::endl;
    }
    return;
}

static const int MAX_PROCESS_ARGC = 8;
static void EnterExec(const std::string &processName)
{
    std::string tmpName = processName;
    std::vector<std::string> vtr;
    const std::string sep = " ";
    OHOS::SplitStr(tmpName, sep, vtr, true, false);

    if ((vtr.size() > MAX_PROCESS_ARGC) || (vtr.size() == 0)) {
        std::cout << "Service parameters is error." << std::endl;
        return;
    }
    char *argv[MAX_PROCESS_ARGC] = {};
    std::vector<std::string>::iterator it;
    int i = 0;
    for (it = vtr.begin(); it != vtr.end(); ++it) {
        argv[i] = (char *)(*it).c_str();
        std::cout << std::string(argv[i]) << std::endl;
        i++;
    }
    argv[i] = NULL;
    char *envp[] = { NULL };
    if (execve(argv[0], argv, envp) != 0) {
        std::cout << "execve:" << argv[0] << "failed! err = "<< errno << std::endl;
    }
    return;
}

static int SendAppspawnCmdMessage(const CmdAgent *agent, uint16_t type, const char *cmd, const char *ptyName)
{
#ifdef ENABLE_ENTER_APPSPAWN_SANDBOX
    if ((agent == NULL) || (cmd == NULL) || (ptyName == NULL)) {
        BEGET_LOGE("Invalid parameter");
        return -1;
    }

    AppSpawnClientHandle clientHandle;
    int ret = AppSpawnClientInit("AppSpawn", &clientHandle);
    BEGET_ERROR_CHECK(ret == 0,  return -1, "AppSpawnClientInit error, errno = %d", errno);
    AppSpawnReqMsgHandle reqHandle;
    ret = AppSpawnReqMsgCreate(AppSpawnMsgType::MSG_BEGET_CMD, cmd, &reqHandle);
    BEGET_ERROR_CHECK(ret == 0, AppSpawnClientDestroy(clientHandle); return -1, "AppSpawnReqMsgCreate error");
    ret = AppSpawnReqMsgSetAppFlag(reqHandle, APP_FLAGS_BEGETCTL_BOOT);
    BEGET_ERROR_CHECK(ret == 0, AppSpawnClientDestroy(clientHandle); return -1, "AppSpawnReqMsgSetAppFlag error");
    ret = AppSpawnReqMsgAddStringInfo(reqHandle, MSG_EXT_NAME_BEGET_PID, cmd);
    BEGET_ERROR_CHECK(ret == 0, AppSpawnClientDestroy(clientHandle); return -1, "add %s request message error", cmd);
    ret = AppSpawnReqMsgAddStringInfo(reqHandle, MSG_EXT_NAME_BEGET_PTY_NAME, ptyName);
    BEGET_ERROR_CHECK(ret == 0, AppSpawnClientDestroy(clientHandle);
        return -1, "add %s request message error", ptyName);
    AppSpawnResult result = {};
    ret = AppSpawnClientSendMsg(clientHandle, reqHandle, &result);
    if (ret != 0 || result.result != 0) {
        AppSpawnClientDestroy(clientHandle);
        return -1;
    }
    AppSpawnClientDestroy(clientHandle);
    return 0;
#endif
    return -1;
}

static void CmdAppspawnClientInit(const char *cmd, CallbackSendMsgProcess sendMsg)
{
    if (cmd == nullptr) {
        BEGET_LOGE("[control_fd] Invalid parameter");
        return;
    }

    CmdAgent agent;
    int ret = InitPtyInterface(&agent, ACTION_APP_ANDBOX, cmd, sendMsg);
    if (ret != 0) {
        BEGET_LOGE("App with pid=%s does not support entering sandbox environment", cmd);
        return;
    }
    LE_RunLoop(LE_GetDefaultLoop());
    LE_CloseLoop(LE_GetDefaultLoop());
    BEGET_LOGI("Cmd Client exit ");
}

static void RunCmd(const std::string &serviceName, const std::string &namespaceName, const std::string &processName,
    const std::string &pid)
{
    bool isNamespaceOnly = !namespaceName.empty() && processName.empty() && serviceName.empty() && pid.empty();
    bool isNamespaceAndProcess = !namespaceName.empty() && !processName.empty() && serviceName.empty() && pid.empty();
    bool isProcessOnly = namespaceName.empty() && !processName.empty() && serviceName.empty() && pid.empty();
    bool isServiceOnly = namespaceName.empty() && processName.empty() && !serviceName.empty() && pid.empty();
    bool isPidOnly = namespaceName.empty() && processName.empty() && serviceName.empty() && !pid.empty();
    if (isNamespaceOnly) {
        RunSandbox(namespaceName);
        EnterShell();
    } else if (isNamespaceAndProcess) {
        RunSandbox(namespaceName);
        EnterExec(processName);
    } else if (isProcessOnly) {
        std::cout << "process name:" << processName << std::endl;
        RunSandbox(std::string("system"));
        EnterExec(processName);
    } else if (isServiceOnly) {
        std::cout << "enter sandbox service name " << serviceName << std::endl;
        CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_SANDBOX, serviceName.c_str(), nullptr);
    } else if (isPidOnly) {
        CmdAppspawnClientInit(pid.c_str(), SendAppspawnCmdMessage);
    } else {
        Usage();
    }
}

static int main_cmd(BShellHandle shell, int argc, char **argv)
{
    int rc = -1;
    int optIndex = -1;
    std::string serviceName {};
    std::string namespaceName {};
    std::string processName {};
    std::string pid {};
    while ((rc = getopt_long(argc, argv, "s:n:p:h:b:",  g_options, &optIndex)) != -1) {
        switch (rc) {
            case 0: {
                std::string optionName = g_options[optIndex].name;
                if (optionName == "service_name") {
                    serviceName = optarg;
                } else if (optionName == "help") {
                    Usage();
                } else if (optionName == "namespace_name") {
                    namespaceName = optarg;
                } else if (optionName == "process_name") {
                    processName = optarg;
                } else if (optionName == "process_pid") {
                    pid = optarg;
                }
                break;
            }
            case 's':
                serviceName = optarg;
                break;
            case 'h':
                Usage();
                break;
            case 'n':
                namespaceName = optarg;
                break;
            case 'p':
                processName = optarg;
                break;
            case 'b':
                pid = optarg;
                break;
            case '?':
                std::cout << "Invalid argument\n";
                break;
            default:
                std::cout << "Invalid argument\n";
                break;
        }
    }
    RunCmd(serviceName, namespaceName, processName, pid);
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {
            const_cast<char *>("sandbox"), main_cmd, const_cast<char *>("enter service sandbox"),
            const_cast<char *>("sandbox -s service_name"),
            NULL
        },
        {
            const_cast<char *>("sandbox"), main_cmd, const_cast<char *>("enter namespace, system, chipset etc."),
            const_cast<char *>("sandbox -n namespace_name [-p]"),
            NULL
        },
        {
            const_cast<char *>("sandbox"), main_cmd, const_cast<char *>("enter namespace and exec process"),
            const_cast<char *>("sandbox -p process_name"),
            NULL
        },
        {
            const_cast<char *>("sandbox"), main_cmd, const_cast<char *>("enter native app sandbox namespace"),
            const_cast<char *>("sandbox -b pid"),
            NULL
        }
    };
    for (size_t i = 0; i < ARRAY_LENGTH(infos); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}
