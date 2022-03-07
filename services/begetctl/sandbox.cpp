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
#include "init_utils.h"
#include "sandbox.h"
#include "sandbox_namespace.h"
#include "string_ex.h"

using namespace OHOS;
struct option g_options[] = {
    { "config_file", required_argument, nullptr, 'c' },
    { "sandbox_name", required_argument, nullptr, 's' },
    { "process_name", required_argument, nullptr, 'p' },
    { "help", no_argument, nullptr, 'h' },
    { nullptr, 0, nullptr, 0 },
};

static void Usage()
{
    std::cout << "sandbox -c, --config_file=sandbox config file \"config file with json format\"" << std::endl;
    std::cout << "sandbox -s, --sandbox_name=sandbox name \"Sandbox name, system, chipset etc.\"" << std::endl;
    std::cout << "sandbox -p, --process=process name \"sh, hdcd, hdf_devhost, etc.\"" << std::endl;
    std::cout << "sandbox -h, --help \"Show help\"" << std::endl;
    exit(0);
}

static std::string SearchConfigBySandboxName(const std::string &sandboxName)
{
    std::map<std::string, std::string> sandboxConfigMap = {
        {"system", "/system/etc/system-sandbox.json"},
        {"chipset", "/system/etc/chipset-sandbox.json"},
        {"priv-app", "/system/etc/priv-app-sandbox.json"},
        {"app", "/system/etc/app-sandbox.json"},
    };
    auto it = sandboxConfigMap.find(sandboxName);
    if (it == sandboxConfigMap.end()) {
        return "";
    } else {
        return it->second;
    }
}

static void RunSandbox(const std::string &configFile, const std::string &name)
{
    std::string config {};
    std::string sandboxName {};
    if (!name.empty()) {
        config = SearchConfigBySandboxName(name);
        sandboxName = name;
    } else {
        // Without sandbox name, give one.
        sandboxName = "sandbox_test";
    }

    if (config.empty()) {
        std::cout << "No sandbox name " << sandboxName << "or config file specified!" << std::endl;
        return;
    }
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
    char *argv[] = { (char *)"sh", NULL };
    char *envp[] = { nullptr };
    if (execve("/system/bin/sh", argv, envp) != 0) {
        std::cout << "execve sh failed! err = "<< errno << std::endl;
    }
    return;
}

static const int MAX_PROCESS_ARGC = 8;
static void EnterExec(const std::string &processName)
{
    if (processName.empty()) {
        std::cout << "process name is nullptr." << std::endl;
        return;
    }
    std::string tmpName = processName;
    std::vector<std::string> vtr;
    const std::string sep = " ";
    OHOS::SplitStr(tmpName, sep, vtr, true, false);

    if ((vtr.size() > MAX_PROCESS_ARGC) || (vtr.size() <= 0)) {
        std::cout << "Service parameters is error." << std::endl;
        return;
    }
    char *argv[MAX_PROCESS_ARGC] = {};
    std::vector<std::string>::iterator it;
    int i = 0;
    for (it = vtr.begin(); it != vtr.end(); it++) {
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

static void RunCmd(const std::string &configFile, const std::string &sandboxName, const std::string &processName)
{
    if (!sandboxName.empty() && processName.empty()) {
        RunSandbox(configFile, sandboxName);
        EnterShell();
    } else if (!sandboxName.empty() && !processName.empty()) {
        RunSandbox(configFile, sandboxName);
        EnterExec(processName);
    } else if (sandboxName.empty() && !processName.empty()) {
        std::cout << "process name:" << processName << std::endl;
        RunSandbox(configFile, std::string("system"));
        EnterExec(processName);
    } else {
        Usage();
    }
}

static int main_cmd(BShellHandle shell, int argc, char **argv)
{
    int rc = -1;
    int optIndex = -1;
    std::string configFile {};
    std::string sandboxName {};
    std::string processName {};
    while ((rc = getopt_long(argc, argv, "c:s:p:h",  g_options, &optIndex)) != -1) {
        switch (rc) {
            case 0: {
                std::string optionName = g_options[optIndex].name;
                if (optionName == "config_file") {
                    configFile = optarg;
                } else if (optionName == "help") {
                    Usage();
                } else if (optionName == "sandbox_name") {
                    sandboxName = optarg;
                } else if (optionName == "process_name") {
                    processName = optarg;
                }
                break;
            }
            case 'c':
                configFile = optarg;
                break;
            case 'h':
                Usage();
                break;
            case 's':
                sandboxName = optarg;
                break;
            case 'p':
                std::cout << "1111 process name:" << optarg << std::endl;
                processName = optarg;
                break;
            case '?':
                std::cout << "Invalid arugment\n";
                break;
            default:
                std::cout << "Invalid arugment\n";
                break;
        }
    }
    RunCmd(configFile, sandboxName, processName);
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    CmdInfo infos[] = {
        {
            (char *)"sandbox", main_cmd, (char *)"sandbox debug tool",
            (char *)"sandbox -s, --sandbox=system, chipset, priv-app, or app",
            NULL
        }
    };
    for (size_t i = 0; i < ARRAY_LENGTH(infos); i++) {
        BShellEnvRegitsterCmd(GetShellHandle(), &infos[i]);
    }
}
