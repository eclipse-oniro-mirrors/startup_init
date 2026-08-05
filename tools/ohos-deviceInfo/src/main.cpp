/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <cJSON.h>

#include "field_registry.h"
#include "output_formatter.h"

namespace {
const char *G_PROGRAM_NAME = "ohos-deviceInfo";
const char *G_TOOL_DESCRIPTION =
    "Query OpenHarmony device information via libbegetutil. Used for device "
    "identification and version diagnostics. Does not support write operations "
    "or cross-device queries.";

const int ARG_INDEX_SUB_COMMAND = 1;
const int ARG_COUNT_WITH_SUB_COMMAND = 2;
const int ARG_SKIP_COUNT = 2;
const int EXIT_FAILURE_CODE = 1;

struct Command {
    const char *name;
    const char *description;
    const char *usage;
    const char *parameters;
    const char *examples;
    std::function<int(int, char **)> handler;
};

std::unordered_map<std::string, Command> g_commands;
bool g_hasSubCommand = false;

void RegisterCommand(const std::string &name, const Command &cmd)
{
    g_commands[name] = cmd;
}

int CmdGet(int argc, char **argv)
{
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) {
        return OutputError("ERR_ARG_MISSING",
            "Missing required parameter: <field>. Context: get <field>",
            "Provide a device info field name. Example: ohos-deviceInfo get osFullName");
    }
    std::string field = argv[0];
    FieldRegistry &registry = FieldRegistry::GetInstance();
    if (!registry.HasField(field)) {
        return OutputError("ERR_ARG_INVALID",
            "Unknown field name '" + field + "'. Context: get <field>",
            "Run 'ohos-deviceInfo all' to list all valid fields. Example: ohos-deviceInfo get osFullName");
    }
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, field.c_str(), registry.InvokeField(field));
    return OutputSuccess(data);
}

int CmdAll(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    FieldRegistry &registry = FieldRegistry::GetInstance();
    cJSON *data = registry.InvokeAll();
    return OutputSuccess(data);
}

void PrintGlobalHelp()
{
    CLI_LOG("%s - %s", G_PROGRAM_NAME, G_TOOL_DESCRIPTION);
    CLI_LOG("");
    CLI_LOG("Usage:");
    CLI_LOG("  %s [options]", G_PROGRAM_NAME);
    CLI_LOG("  %s <command> [options]", G_PROGRAM_NAME);
    CLI_LOG("");
    CLI_LOG("Parameters:");
    CLI_LOG("  --help             Display this help message");
    CLI_LOG("");
    CLI_LOG("SubCommands:");
    CLI_LOG("  get                Query a single device info field by name");
    CLI_LOG("  all                Query all device info fields at once");
    CLI_LOG("");
    CLI_LOG("Examples:");
    CLI_LOG("  %s --help", G_PROGRAM_NAME);
    CLI_LOG("  %s get osFullName", G_PROGRAM_NAME);
    CLI_LOG("  %s get displayVersion", G_PROGRAM_NAME);
    CLI_LOG("  %s all", G_PROGRAM_NAME);
}

void PrintCommandHelp(const Command &cmd)
{
    CLI_LOG("%s %s - %s", G_PROGRAM_NAME, cmd.name, cmd.description);
    if (cmd.usage != nullptr) {
        CLI_LOG("");
        CLI_LOG("Usage:");
        CLI_LOG("  %s", cmd.usage);
    }
    if (cmd.parameters != nullptr) {
        CLI_LOG("");
        CLI_LOG("Parameters:");
        CLI_LOG("%s", cmd.parameters);
    }
    CLI_LOG("    --help             Display this help message");
    if (cmd.examples != nullptr) {
        CLI_LOG("");
        CLI_LOG("Examples:");
        CLI_LOG("%s", cmd.examples);
    }
}

int CmdHelp(int argc, char **argv)
{
    std::string targetCmd;
    for (int i = 0; i < argc; i++) {
        if (argv[i] != nullptr && argv[i][0] != '-') {
            targetCmd = argv[i];
            break;
        }
    }

    if (targetCmd.empty()) {
        PrintGlobalHelp();
        return 0;
    }

    auto it = g_commands.find(targetCmd);
    if (it == g_commands.end()) {
        return OutputError("ERR_ARG_INVALID",
            "Unknown command: " + targetCmd,
            "Run 'ohos-deviceInfo --help' to list available commands.");
    }
    PrintCommandHelp(it->second);
    return 0;
}

void InitCommands()
{
    Command getCmd;
    getCmd.name = "get";
    getCmd.description = "Query a single device info field by name. Used for "
        "inspecting one specific device attribute. Does not support multiple "
        "fields in one call (use 'all' instead).";
    getCmd.usage = "ohos-deviceInfo get <field>";
    getCmd.parameters =
        "    <field>          Required. Device info field name, e.g.\n"
        "                     osFullName, displayVersion, sdkApiVersion.\n"
        "                     Run 'ohos-deviceInfo all' to list all valid\n"
        "                     fields.";
    getCmd.examples =
        "    ohos-deviceInfo get osFullName\n"
        "    ohos-deviceInfo get displayVersion\n"
        "    ohos-deviceInfo get sdkApiVersion";
    getCmd.handler = CmdGet;
    RegisterCommand("get", getCmd);

    Command allCmd;
    allCmd.name = "all";
    allCmd.description = "Query all device info fields at once. Used for full "
        "device profiling and diagnostics. Does not accept any parameters.";
    allCmd.usage = "ohos-deviceInfo all";
    allCmd.parameters =
        "    (none)           No parameters accepted.";
    allCmd.examples =
        "    ohos-deviceInfo all";
    allCmd.handler = CmdAll;
    RegisterCommand("all", allCmd);

    Command helpCmd;
    helpCmd.name = "help";
    helpCmd.description = "Show this help message.";
    helpCmd.usage = "ohos-deviceInfo help [command]";
    helpCmd.parameters =
        "    command          Optional. Show detailed help for a specific command.";
    helpCmd.examples =
        "    ohos-deviceInfo help\n"
        "    ohos-deviceInfo help get";
    helpCmd.handler = CmdHelp;
    RegisterCommand("help", helpCmd);

    g_hasSubCommand = (g_commands.size() > 1);
}

void PrintUsage(const char *prog)
{
    CLI_ERROR("Usage: %s <command> [options...]", prog);
    CLI_ERROR("Run '%s --help' for more information", prog);
}
}  // namespace

int main(int argc, char **argv)
{
    if (argc >= ARG_COUNT_WITH_SUB_COMMAND && strcmp(argv[ARG_INDEX_SUB_COMMAND], "--help") == 0) {
        InitCommands();
        char *helpArgv[] = {nullptr};
        CmdHelp(0, helpArgv);
        return 0;
    }

    if (argc < ARG_COUNT_WITH_SUB_COMMAND) {
        PrintUsage(G_PROGRAM_NAME);
        return EXIT_FAILURE_CODE;
    }

    InitCommands();

    std::string cmdName = argv[ARG_INDEX_SUB_COMMAND];

    for (int i = ARG_SKIP_COUNT; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            char *helpArgv[] = {const_cast<char *>(cmdName.c_str()), nullptr};
            CmdHelp(ARG_INDEX_SUB_COMMAND, helpArgv);
            return 0;
        }
    }

    auto it = g_commands.find(cmdName);
    if (it == g_commands.end()) {
        return OutputError("ERR_ARG_INVALID",
            "Unknown command: " + cmdName + ". The command is not supported.",
            "Run '" + std::string(G_PROGRAM_NAME) + " --help' to see available commands.");
    }

    return it->second.handler(argc - ARG_SKIP_COUNT, argv + ARG_SKIP_COUNT);
}
