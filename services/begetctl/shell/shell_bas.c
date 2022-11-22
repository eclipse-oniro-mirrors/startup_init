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
#include "shell_bas.h"
#include <fcntl.h>
#include <signal.h>

#include "init_utils.h"
#include "shell_utils.h"

char *BShellEnvErrString(BShellHandle handle, int32_t err)
{
    static BShellErrInfo shellErrString[] = {
        {BSH_SHELL_INFO, "\r\n\r\n"
                        "+=========================================================+\r\n"
                        "|               Parameter shell v"BSH_VERSION"                    |\r\n"
                        "+=========================================================+\r\n"
        },
        {BSH_CMD_TOO_LONG, "\r\nWarnig: Command is too long\r\n"},
        {BSH_SHOW_CMD_LIST, "Command list:\r\n"},
        {BSH_CMD_NOT_EXIST, "Command not found\r\n"}
    };
    for (size_t i = 0; i < sizeof(shellErrString) / sizeof(shellErrString[0]); i++) {
        if ((int32_t)shellErrString[i].err == err) {
            return shellErrString[i].desc;
        }
    }
    BSH_CHECK(handle != NULL, return "System unknown err", "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    int len = sprintf_s(shell->data, sizeof(shell->data) - 1, "System unknown err 0x%08x", err);
    if (len <= 0) {
        BSH_LOGE("Write shell data size failed.");
    }
    return shell->data;
}

static void BShellCmdOutputCmdHelp(BShellHandle handle, BShellCommand *cmd)
{
    BShellEnvOutputString(handle, "    ");
    int32_t spaceLength = BShellEnvOutputString(handle, cmd->help);
    spaceLength = BSH_CMD_NAME_END - spaceLength;
    spaceLength = (spaceLength > 0) ? spaceLength : 4; // 4 min
    do {
        BShellEnvOutputString(handle, " ");
    } while (--spaceLength);
    BShellEnvOutputString(handle, "--");
    BShellEnvOutputString(handle, cmd->desc);
    BShellEnvOutputString(handle, "\r\n");
}

int32_t BShellCmdHelp(BShellHandle handle, int32_t argc, char *argv[])
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    BShellEnvOutputString(handle, BShellEnvErrString(handle, BSH_SHOW_CMD_LIST));

    int show = 0;
    BShellCommand *cmd = shell->command;
    while (cmd != NULL) {
        if ((argc >= 1) &&
            (strncmp(argv[0], cmd->name, strlen(argv[0])) == 0) &&
            (strncmp(argv[0], "help", strlen(argv[0])) != 0)) {
            BShellCmdOutputCmdHelp(handle, cmd);
            show = 1;
        }
        cmd = cmd->next;
    }
    if (show) {
        return 0;
    }
    cmd = shell->command;
    while (cmd != NULL) {
        BShellCmdOutputCmdHelp(handle, cmd);
        cmd = cmd->next;
    }
    return 0;
}

static int32_t BShellCmdExit(BShellHandle handle, int32_t argc, char *argv[])
{
#ifndef STARTUP_INIT_TEST
    kill(getpid(), SIGINT);
#endif
    return 0;
}

int32_t BShellEnvOutput(BShellHandle handle, char *fmt, ...)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    va_list list;
    va_start(list, fmt);
    int len = vfprintf(stdout, fmt, list);
    va_end(list);
    (void)fflush(stdout);
    return len;
}

int32_t BShellEnvOutputString(BShellHandle handle, const char *string)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    printf("%s", string);
    (void)fflush(stdout);
    return strlen(string);
}

int32_t BShellEnvOutputPrompt(BShellHandle handle, const char *prompt)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_CHECK(prompt != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    if (shell->prompt != NULL) {
        free(shell->prompt);
    }
    size_t promptLen = strlen(prompt);
    if (promptLen > BSH_CMD_NAME_END) {
        shell->prompt = strdup(prompt + promptLen - BSH_CMD_NAME_END);
        if (shell->prompt != NULL) {
            shell->prompt[0] = '.';
            shell->prompt[1] = '.';
            shell->prompt[2] = '.'; // 2 index
        }
    } else {
        shell->prompt = strdup(prompt);
    }
    return 0;
}

void BShellEnvOutputByte(BShellHandle handle, char data)
{
    BSH_CHECK(handle != NULL, return, "Invalid shell env");
    printf("%c", data);
    (void)fflush(stdout);
}

void BShellEnvOutputResult(BShellHandle handle, int32_t result)
{
    if (result == 0) {
        return;
    }
    printf("result: 0x%08x\r\n", result);
    (void)fflush(stdout);
}

static void BShellEnvOutputParam(BShellHandle handle, char *var)
{
    BShellEnvOutput(handle, (var[0] == '$') ? var + 1 : var);
    BShellEnvOutputString(handle, " = ");
    BShellEnvOutputString(handle, BShellEnvGetStringParam(handle, var));
}

void BShellEnvBackspace(BShellHandle handle, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++) {
        BShellEnvOutputString(handle, "\b \b");
    }
}

static void BShellEnvParseParam(BShellEnv *shell)
{
    uint8_t quotes = 0;
    uint8_t record = 1;
    shell->argc = 0;
    for (uint16_t i = 0; i < shell->length; i++) {
        char data = *(shell->buffer + i);
        if ((quotes != 0 || (data != ' ' && data != '\t' && data != ',')) && data != 0) {
            if (data == '\"') {
                quotes = quotes ? 0 : 1;
            }
            if (record == 1) {
                BSH_CHECK(shell->argc < BSH_PARAMETER_MAX_NUMBER, return, "argc out of range");
                shell->args[shell->argc++] = shell->buffer + i;
                record = 0;
            }
            if (*(shell->buffer + i) == '\\' && *(shell->buffer + i + 1) != 0) {
                i++;
            }
        } else {
            *(shell->buffer + i) = 0;
            record = 1;
        }
    }
}

static int32_t BShellEnvExcuteCmd(BShellEnv *shell, BShellCommand *cmd)
{
    return cmd->executer((BShellHandle)shell, shell->argc - cmd->argStart, &shell->args[cmd->argStart]);
}

static int32_t BShellEnvHandleEnter(BShellHandle handle, uint8_t data)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    if (shell->length == 0) {
        BShellEnvOutputString(shell, "\n");
        BShellEnvOutputString(handle, shell->prompt);
        return 0;
    }

    *(shell->buffer + shell->length++) = 0;
    BShellEnvParseParam(shell);
    shell->length = 0;
    shell->cursor = 0;
    if (shell->argc == 0) {
        BShellEnvOutputString(shell, shell->prompt);
        return 0;
    }

    BShellEnvOutputString(shell, "\n");
    if (strcmp((const char *)shell->args[0], "help") == 0) {
        BShellCmdHelp(handle, shell->argc, shell->args);
        BShellEnvOutputString(shell, shell->prompt);
        return 0;
    }
    if (shell->args[0][0] == '$') {
        BShellEnvOutputParam(shell, shell->args[0]);
        BShellEnvOutputString(shell, shell->prompt);
        return 0;
    }

    BShellCommand *cmd = BShellEnvGetCmd(handle, (uint32_t)shell->argc, shell->args);
    if (cmd != NULL) {
        int32_t ret = BShellEnvExcuteCmd(shell, cmd);
        BShellEnvOutputResult(shell, ret);
    } else {
        BShellEnvOutputString(shell, BShellEnvErrString(handle, BSH_CMD_NOT_EXIST));
    }
    BShellEnvOutputString(shell, shell->prompt);
    return 0;
}

static int32_t BShellEnvHandleBackspace(BShellHandle handle, uint8_t code)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    if (shell->length == 0) {
        return 0;
    }
    if (shell->cursor == shell->length) {
        shell->length--;
        shell->cursor--;
        shell->buffer[shell->length] = 0;
        BShellEnvBackspace(handle, 1);
    } else if (shell->cursor > 0) {
        for (short i = 0; i < shell->length - shell->cursor; i++) {
            shell->buffer[shell->cursor + i - 1] = shell->buffer[shell->cursor + i];
        }
        shell->length--;
        shell->cursor--;
        shell->buffer[shell->length] = 0;
        BShellEnvOutputByte(shell, '\b');
        for (short i = shell->cursor; i < shell->length; i++) {
            BShellEnvOutputByte(shell, shell->buffer[i]);
        }
        BShellEnvOutputByte(shell, ' ');
        for (short i = shell->length - shell->cursor + 1; i > 0; i--) {
            BShellEnvOutputByte(shell, '\b');
        }
    }
    return 0;
}

static int32_t BShellEnvHandleTab(BShellHandle handle, uint8_t code)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    return 0;
}

static void BShellEnvHandleNormal(BShellHandle handle, uint8_t data)
{
    BSH_CHECK(handle != NULL, return, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    if (data == 0) {
        return;
    }
    if (shell->length < BSH_COMMAND_MAX_LENGTH - 1) {
        if (shell->length == shell->cursor) {
            shell->buffer[shell->length++] = data;
            shell->cursor++;
            BShellEnvOutputByte(shell, data);
        } else {
            for (uint16_t i = shell->length - shell->cursor; i > 0; i--) {
                shell->buffer[shell->cursor + i] = shell->buffer[shell->cursor + i - 1];
            }
            shell->buffer[shell->cursor++] = data;
            shell->buffer[++shell->length] = 0;
            for (uint16_t i = shell->cursor - 1; i < shell->length; i++) {
                BShellEnvOutputByte(shell, shell->buffer[i]);
            }
            for (uint16_t i = shell->length - shell->cursor; i > 0; i--) {
                BShellEnvOutputByte(shell, '\b');
            }
        }
    } else {
        BShellEnvOutputString(shell, BShellEnvErrString(handle, BSH_CMD_TOO_LONG));
        BShellEnvOutputString(shell, shell->prompt);

        shell->cursor = shell->length;
    }
}

static int32_t BShellEnvHandleCtrC(BShellHandle handle, uint8_t code)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_LOGV("BShellEnvHandleCtrC %d", getpid());
#ifndef STARTUP_INIT_TEST
    kill(getpid(), SIGKILL);
#endif
    return 0;
}

static int32_t BShellEnvHandleEsc(BShellHandle handle, uint8_t code)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    shell->shellState = BSH_ANSI_ESC;
    return 0;
}

BShellKey *BShellEnvGetDefaultKey(uint8_t code)
{
    static BShellKey defaultKeys[] = {
        {BSH_KEY_LF, BShellEnvHandleEnter, NULL},
        {BSH_KEY_CR, BShellEnvHandleEnter, NULL},
        {BSH_KEY_TAB, BShellEnvHandleTab, NULL},
        {BSH_KEY_BACKSPACE, BShellEnvHandleBackspace, NULL},
        {BSH_KEY_DELETE, BShellEnvHandleBackspace, NULL},
        {BSH_KEY_CTRLC, BShellEnvHandleCtrC, NULL},
        {BSH_KEY_ESC, BShellEnvHandleEsc, NULL},
    };
    for (size_t i = 0; i < sizeof(defaultKeys) / sizeof(defaultKeys[0]); i++) {
        if (defaultKeys[i].keyCode == code) {
            return &defaultKeys[i];
        }
    }
    return NULL;
}

SHELLSTATIC void BShellEnvProcessInput(BShellHandle handle, char data)
{
    BSH_CHECK(handle != NULL, return, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    if (shell->shellState == BSH_IN_NORMAL) {
        BShellKey *key = BShellEnvGetKey(handle, data);
        if (key != NULL) {
            key->keyHandle(shell, (uint8_t)data);
            return;
        }
        key = BShellEnvGetDefaultKey(data);
        if (key != NULL) {
            key->keyHandle(shell, (uint8_t)data);
            return;
        }
        BShellEnvHandleNormal(shell, data);
    } else if (shell->shellState == BSH_ANSI_CSI) {
        switch (data) {
            case 0x41:  // up
                break;
            case 0x42:  // down
                break;
            case 0x43:  // ->
                if (shell->cursor < shell->length) {
                    BShellEnvOutputByte(handle, shell->buffer[shell->cursor]);
                    shell->cursor++;
                }
                break;
            case 0x44:  // <-
                if (shell->cursor > 0) {
                    BShellEnvOutputByte(shell, '\b');
                    shell->cursor--;
                }
                break;
            default:
                break;
        }
        shell->shellState = BSH_IN_NORMAL;
    } else if (shell->shellState == BSH_ANSI_ESC) {
        if (data == 0x5B) { // input up/down
            shell->shellState = BSH_ANSI_CSI;
        } else {
            shell->shellState = BSH_IN_NORMAL;
        }
    }
}

void BShellEnvLoop(BShellHandle handle)
{
    BSH_CHECK(handle != NULL, return, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    BSH_CHECK(shell->input != NULL, return, "Invalid shell input");
    while (1) {
        char data = 0;
        if (shell->input(&data, 1) == 1) {
            BShellEnvProcessInput(shell, data);
        }
    }
}

int32_t BShellEnvInit(BShellHandle *handle, const BShellInfo *info)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_CHECK(info != NULL && info->prompt != NULL, return BSH_INVALID_PARAM, "Invalid cmd name");

    BShellEnv *shell = (BShellEnv *)calloc(1, sizeof(BShellEnv));
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Failed to create shell env");
    shell->length = 0;
    shell->cursor = 0;
    shell->shellState = BSH_IN_NORMAL;
    shell->input = info->input;
    shell->prompt = strdup(info->prompt);
    shell->command = NULL;
    shell->param = NULL;
    shell->keyHandle = NULL;
    shell->execMode = BSH_EXEC_INDEPENDENT;
    *handle = (BShellHandle)shell;
    return 0;
}

int BShellEnvStart(BShellHandle handle)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    shell->execMode = BSH_EXEC_TASK;
    BShellEnvOutputString(handle, BShellEnvErrString(handle, BSH_SHELL_INFO));
    BShellEnvOutputString(handle, shell->prompt);

    const CmdInfo infos[] = {
        {"exit", BShellCmdExit, "exit parameter shell", "exit"},
        {"help", BShellCmdHelp, "help command", "help"}
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(handle, &infos[i]);
    }
    return 0;
}

static void BShellParamFree(BShellParam *param)
{
    if (param->desc != NULL) {
        free(param->desc);
    }
    if (param->type == PARAM_STRING && param->value.string != NULL) {
        free(param->value.string);
    }
    free(param);
}

static void BShellCmdFree(BShellCommand *cmd)
{
    if (cmd->desc != NULL) {
        free(cmd->desc);
        cmd->desc = NULL;
    }
    if (cmd->help != NULL) {
        free(cmd->help);
        cmd->help = NULL;
    }
    if (cmd->multikey != NULL) {
        free(cmd->multikey);
        cmd->multikey = NULL;
    }
    free(cmd);
}

void BShellEnvDestory(BShellHandle handle)
{
    BSH_CHECK(handle != NULL, return, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;

    BShellCommand *cmd = shell->command;
    while (cmd != NULL) {
        shell->command = cmd->next;
        BShellCmdFree(cmd);
        cmd = shell->command;
    }

    BShellParam *param = shell->param;
    while (param != NULL) {
        shell->param = param->next;
        BShellParamFree(param);
        param = shell->param;
    }

    BShellKey *key = shell->keyHandle;
    while (key != NULL) {
        shell->keyHandle = key->next;
        free(key);
        key = shell->keyHandle;
    }
    if (shell->prompt != NULL) {
        free(shell->prompt);
    }
    free(shell);
}

int32_t BShellEnvRegisterCmd(BShellHandle handle, const CmdInfo *cmdInfo)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_CHECK(cmdInfo != NULL && cmdInfo->name != NULL, return BSH_INVALID_PARAM, "Invalid cmd name");
    BSH_CHECK(cmdInfo->executer != NULL, return BSH_INVALID_PARAM, "Invalid cmd executer");
    BShellEnv *shell = (BShellEnv *)handle;
    size_t nameLen = strlen(cmdInfo->name) + 1;
    BShellCommand *cmd = (BShellCommand *)calloc(1, sizeof(BShellCommand) + nameLen);
    BSH_CHECK(cmd != NULL, return BSH_INVALID_PARAM, "Failed to alloc cmd name %s", cmdInfo->name);
    cmd->executer = cmdInfo->executer;
    cmd->argStart = 0;
    int32_t ret = 0;
    do {
        ret = strcpy_s(cmd->name, nameLen, cmdInfo->name);
        BSH_CHECK(ret == 0, break, "Failed to copy name %s", cmdInfo->name);

        ret = BSH_SYSTEM_ERR;
        if (cmdInfo->desc != NULL) {
            cmd->desc = strdup(cmdInfo->desc);
            BSH_CHECK(cmd->desc != NULL, break, "Failed to copy desc %s", cmdInfo->name);
        }
        if (cmdInfo->help != NULL) {
            cmd->help = strdup(cmdInfo->help);
            BSH_CHECK(cmd->help != NULL, break, "Failed to copy help %s", cmdInfo->name);
        }
        cmd->multikey = NULL;
        if (cmdInfo->multikey != NULL && strlen(cmdInfo->multikey) > nameLen) {
            cmd->multikey = strdup(cmdInfo->multikey);
            BSH_CHECK(cmd->multikey != NULL, break, "Failed to copy multikey %s", cmdInfo->name);
            int argc = SplitString(cmd->multikey, " ", cmd->multikeys, (int)ARRAY_LENGTH(cmd->multikeys));
            BSH_CHECK(argc <= (int)ARRAY_LENGTH(cmd->multikeys) && argc > 0, break, "Invalid multikey");
            cmd->argStart = argc - 1;
            if (argc == 1) {
                free(cmd->multikey);
                cmd->multikey = NULL;
            }
        }
        ret = 0;
    } while (0);
    if (ret != 0) {
        BShellCmdFree(cmd);
        return ret;
    }
    cmd->next = shell->command;
    shell->command = cmd;
    return 0;
}

static const char *GetRealCmdName(const char *name)
{
    int i = 0;
    int last = 0;
    while (*(name + i) != '\0') {
        if (*(name + i) == '/') {
            last = i;
        }
        i++;
    }
    if (last != 0) {
        return name + last + 1;
    } else {
        return name;
    }
}

BShellCommand *BShellEnvGetCmd(BShellHandle handle, int32_t argc, char *argv[])
{
    BSH_CHECK(handle != NULL, return NULL, "Invalid shell env");
    BSH_CHECK(argc >= 1, return NULL, "Invalid argc");
    const char *cmdName = GetRealCmdName(argv[0]);
    BSH_LOGV("BShellEnvGetCmd %s cmd %s", argv[0], cmdName);
    BShellEnv *shell = (BShellEnv *)handle;
    BShellCommand *cmd = shell->command;
    while (cmd != NULL) {
        if (strcmp(cmd->name, cmdName) != 0) {
            cmd = cmd->next;
            continue;
        }
        if (cmd->multikey == NULL) {
            return cmd;
        }
        int32_t i = 0;
        for (i = 0; i < (int32_t)ARRAY_LENGTH(cmd->multikeys) && i < argc; i++) {
            if (cmd->multikeys[i] == NULL) {
                return cmd;
            }
            char *tmp = argv[i];
            if (i == 0) {
                tmp = (char *)cmdName;
            }
            if (strcmp(cmd->multikeys[i], tmp) != 0) {
                break;
            }
        }
        if (i >= (int32_t)ARRAY_LENGTH(cmd->multikeys)) {
            return cmd;
        }
        if (i >= argc) {
            if (cmd->multikeys[i] == NULL) {
                return cmd;
            }
        }
        cmd = cmd->next;
    }
    return NULL;
}

int32_t BShellEnvRegisterKeyHandle(BShellHandle handle, uint8_t code, BShellkeyHandle keyHandle)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_CHECK(keyHandle != NULL, return BSH_INVALID_PARAM, "Invalid cmd name");
    BShellEnv *shell = (BShellEnv *)handle;

    BShellKey *key = (BShellKey *)calloc(1, sizeof(BShellKey));
    BSH_CHECK(key != NULL, return BSH_INVALID_PARAM, "Failed to alloc key code %d", code);
    key->keyCode = code;
    key->keyHandle = keyHandle;
    key->next = shell->keyHandle;
    shell->keyHandle = key;
    return 0;
}

BShellKey *BShellEnvGetKey(BShellHandle handle, uint8_t code)
{
    BSH_CHECK(handle != NULL, return NULL, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    BShellKey *key = shell->keyHandle;
    while (key != NULL) {
        if (key->keyCode == code) {
            return key;
        }
        key = key->next;
    }
    return NULL;
}

static int32_t BShellParamSetValue(BShellParam *param, void *value)
{
    static uint32_t paramValueLens[] = {
        sizeof(uint8_t), sizeof(uint16_t), sizeof(uint32_t), sizeof(char *)
    };
    if (param->type == PARAM_STRING) {
        param->value.string = strdup((char *)value);
        BSH_CHECK(param->value.string != NULL, return BSH_SYSTEM_ERR, "Failed to copy value for %s", param->name);
    } else if (param->type < PARAM_STRING) {
        int ret = memcpy_s(&param->value, sizeof(param->value), value, paramValueLens[param->type]);
        BSH_CHECK(ret == 0, return BSH_SYSTEM_ERR, "Failed to copy value for %s", param->name);
    }
    return 0;
}

int32_t BShellEnvSetParam(BShellHandle handle, const char *name, const char *desc, BShellParamType type, void *value)
{
    BSH_CHECK(handle != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_CHECK(name != NULL, return BSH_INVALID_PARAM, "Invalid param name");
    BSH_CHECK(value != NULL, return BSH_INVALID_PARAM, "Invalid cmd value");
    BSH_CHECK(type <= PARAM_STRING, return BSH_INVALID_PARAM, "Invalid param type");
    BShellEnv *shell = (BShellEnv *)handle;
    const BShellParam *tmp = BShellEnvGetParam(handle, name);
    if (tmp != NULL) {
        BSH_CHECK(tmp->type <= type, return BSH_INVALID_PARAM, "Invalid param type %d", tmp->type);
        return BShellParamSetValue((BShellParam *)tmp, value);
    }
    int32_t ret = 0;
    BShellParam *param = NULL;
    do {
        size_t nameLen = strlen(name) + 1;
        param = (BShellParam *)calloc(1, sizeof(BShellParam) + nameLen);
        BSH_CHECK(param != NULL, return BSH_SYSTEM_ERR, "Failed to alloc cmd name %s", name);
        param->type = type;
        ret = strcpy_s(param->name, nameLen, name);
        BSH_CHECK(ret == 0, break, "Failed to copy name %s", name);
        if (desc != NULL) {
            param->desc = strdup(desc);
        }
        ret = BShellParamSetValue(param, value);
        BSH_CHECK(ret == 0, break, "Failed set value for %s", name);

        param->next = shell->param;
        shell->param = param;
    } while (0);
    if (ret != 0) {
        BShellParamFree(param);
    }
    return ret;
}

const BShellParam *BShellEnvGetParam(BShellHandle handle, const char *name)
{
    BSH_CHECK(handle != NULL, return NULL, "Invalid shell env");
    BShellEnv *shell = (BShellEnv *)handle;
    BShellParam *param = shell->param;
    while (param != NULL) {
        if (strcmp(name, param->name) == 0) {
            return param;
        }
        param = param->next;
    }
    return NULL;
}

const char *BShellEnvGetStringParam(BShellHandle handle, const char *name)
{
    BSH_CHECK(handle != NULL, return "", "Invalid shell env");
    const BShellParam *param = BShellEnvGetParam(handle, name);
    if (param == NULL) {
        return "";
    }
    switch (param->type) {
        case PARAM_STRING:
            return param->value.string;
        default:
            break;
    }
    return "";
}

const ParamInfo *BShellEnvGetReservedParam(BShellHandle handle, const char *name)
{
    BSH_CHECK(handle != NULL, return NULL, "Invalid shell env");
    static ParamInfo reservedParams[] = {
        {PARAM_REVERESD_NAME_CURR_PARAMETER, "current parameter", PARAM_STRING}
    };
    for (size_t i = 0; i < sizeof(reservedParams) / sizeof(reservedParams[0]); i++) {
        if (strcmp(name, reservedParams[i].name) == 0) {
            return &reservedParams[i];
        }
    }
    return NULL;
}

int32_t BShellEnvDirectExecute(BShellHandle handle, int argc, char *args[])
{
    BSH_CHECK(handle != NULL, return -1, "Invalid shell env");
    BShellCommand *cmd = BShellEnvGetCmd(handle, argc, args);
    if (cmd != NULL) {
        int32_t ret = cmd->executer(handle, argc - cmd->argStart, &args[cmd->argStart]);
        BShellEnvOutputResult(handle, ret);
    } else {
        if (argc > 1) {
            BShellEnvOutputString(handle, BShellEnvErrString(handle, BSH_CMD_NOT_EXIST));
        }
        BShellCmdHelp(handle, argc, args);
    }
    return 0;
}
