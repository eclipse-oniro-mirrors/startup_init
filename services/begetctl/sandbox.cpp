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
#include <string>
#include <unistd.h>
#include <vector>

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
    { "help", no_argument, nullptr, 'h' },
    { nullptr, 0, nullptr, 0 },
};

static void Usage()
{
    std::cout << "sandbox -s | -n [-p] | -p | -h" << std::endl;
    std::cout << "sandbox -s, --service_name=sandbox service \"enter service sandbox\"" << std::endl;
    std::cout << "sandbox -n, --namespace_name=namespace name \"namespace name, system, chipset etc.\"" << std::endl;
    std::cout << "sandbox -p, --process=process name \"sh, hdcd, hdf_devhost, etc.\"" << std::endl;
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

static void RunCmd(const std::string &serviceName, const std::string &namespaceName, const std::string &processName)
{
    if (!namespaceName.empty() && processName.empty() && serviceName.empty()) {
        RunSandbox(namespaceName);
        EnterShell();
    } else if (!namespaceName.empty() && !processName.empty() && serviceName.empty()) {
        RunSandbox(namespaceName);
        EnterExec(processName);
    } else if (namespaceName.empty() && !processName.empty() && serviceName.empty()) {
        std::cout << "process name:" << processName << std::endl;
        RunSandbox(std::string("system"));
        EnterExec(processName);
    } else if (namespaceName.empty() && processName.empty() && !serviceName.empty()) {
        std::cout << "enter sandbox service name " << serviceName << std::endl;
        CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_SANDBOX, serviceName.c_str());
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
    while ((rc = getopt_long(argc, argv, "s:n:p:h",  g_options, &optIndex)) != -1) {
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
                std::cout << "process name:" << optarg << std::endl;
                processName = optarg;
                break;
            case '?':
                std::cout << "Invalid argument\n";
                break;
            default:
                std::cout << "Invalid argument\n";
                break;
        }
    }
    RunCmd(serviceName, namespaceName, processName);
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
        }
    };
    for (size_t i = 0; i < ARRAY_LENGTH(infos); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}
