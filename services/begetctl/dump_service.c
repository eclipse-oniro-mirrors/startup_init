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
#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_ENTER_APPSPAWN_SANDBOX
#include "appspawn.h"
#endif
#include "begetctl.h"
#include "beget_ext.h"
#include "control_fd.h"
#include "securec.h"
#include "init_param.h"

#define DUMP_APPSPAWN_CMD_ARGS 1
#define DUMP_SERVICE_INFO_CMD_ARGS 2
#define DUMP_SERVICE_BOOTEVENT_CMD_ARGS 3

static int SendAppspawnCmdMessage(const CmdAgent *agent, uint16_t type, const char *cmd, const char *ptyName)
{
#ifdef ENABLE_ENTER_APPSPAWN_SANDBOX
    if ((agent == NULL) || (cmd == NULL) || (ptyName == NULL)) {
        BEGET_LOGE("Invalid parameter");
        return -1;
    }

    int ret = -1;
    AppSpawnClientHandle clientHandle;
    if (strcmp(cmd, "dump_appspawn") == 0) {
        ret = AppSpawnClientInit("AppSpawn", &clientHandle);
    } else if (strcmp(cmd, "dump_nwebspawn") == 0) {
        ret = AppSpawnClientInit("NWebSpawn", &clientHandle);
    } else {
        BEGET_LOGE("Invalid parameter to dump appspawn");
    }
    BEGET_ERROR_CHECK(ret == 0, return -1, "AppSpawnClientInit error, errno = %d", errno);
    AppSpawnReqMsgHandle reqHandle;
    ret = AppSpawnReqMsgCreate(MSG_DUMP, ptyName, &reqHandle);
    BEGET_ERROR_CHECK(ret == 0, AppSpawnClientDestroy(clientHandle);
        return -1, "AppSpawnReqMsgCreate error");
    ret = AppSpawnReqMsgAddStringInfo(reqHandle, "pty-name", ptyName);
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

static void DumpAppspawnClientInit(const char *cmd, CallbackSendMsgProcess sendMsg)
{
    if (cmd == NULL) {
        BEGET_LOGE("[control_fd] Invalid parameter");
        return;
    }

    CmdAgent agent;
    int ret = InitPtyInterface(&agent, ACTION_DUMP, cmd, sendMsg);
    if (ret != 0) {
        BEGET_LOGE("App with pid=%s does not support entering sandbox environment", cmd);
        return;
    }
    LE_RunLoop(LE_GetDefaultLoop());
    LE_CloseLoop(LE_GetDefaultLoop());
    BEGET_LOGI("Cmd Client exit ");
}

static int main_cmd(BShellHandle shell, int argc, char **argv)
{
    if (argc == DUMP_APPSPAWN_CMD_ARGS) {
        DumpAppspawnClientInit(argv[0], SendAppspawnCmdMessage);
    } else if (argc == DUMP_SERVICE_INFO_CMD_ARGS) {
        printf("dump service info \n");
        CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_DUMP, argv[1], NULL);
    } else if (argc == DUMP_SERVICE_BOOTEVENT_CMD_ARGS) {
        if (strcmp(argv[1], "parameter_service") == 0) {
            printf("dump parameter service info \n");
        }
        size_t serviceNameLen = strlen(argv[1]) + strlen(argv[2]) + 2; // 2 is \0 and #
        char *cmd = (char *)calloc(1, serviceNameLen);
        BEGET_ERROR_CHECK(cmd != NULL, return 0, "failed to allocate cmd memory");
        BEGET_ERROR_CHECK(sprintf_s(cmd, serviceNameLen, "%s#%s", argv[1], argv[2]) >= 0, free(cmd);
            return 0, "dump service arg create failed");
        CmdClientInit(INIT_CONTROL_FD_SOCKET_PATH, ACTION_DUMP, cmd, NULL);
        free(cmd);
    } else {
        BShellCmdHelp(shell, argc, argv);
    }
    return 0;
}

static int BootEventEnable(BShellHandle shell, int argc, char **argv)
{
    if (SystemSetParameter("persist.init.bootevent.enable", "true") == 0) {
        printf("bootevent enabled\n");
    }
    return 0;
}
static int BootEventDisable(BShellHandle shell, int argc, char **argv)
{
    if (SystemSetParameter("persist.init.bootevent.enable", "false") == 0) {
        printf("bootevent disabled\n");
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {"dump_service", main_cmd, "dump all loop info", "dump_service loop", NULL},
        {"dump_service", main_cmd, "dump one service info by serviceName", "dump_service serviceName", NULL},
        {"dump_service", main_cmd, "dump all services info", "dump_service all", NULL},
        {"dump_service", main_cmd, "dump parameter-service trigger",
            "dump_service parameter_service trigger", NULL},
        {"bootevent", BootEventEnable, "bootevent enable", "bootevent enable",
            "bootevent enable"},
        {"bootevent", BootEventDisable, "bootevent disable", "bootevent disable",
            "bootevent disable"},
        {"dump_appspawn", main_cmd, "dump appspawn info", "dump_appspawn", NULL },
        {"dump_nwebspawn", main_cmd, "dump nwebspawn info", "dump_nwebspawn", NULL}
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}
