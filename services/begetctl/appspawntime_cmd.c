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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "begetctl.h"
#include "init_utils.h"
#include "control_fd.h"
#ifdef ENABLE_ENTER_APPSPAWN_SANDBOX
#include "appspawn.h"
#endif

#define APPSPAWN_CMD_NUMBER 1

static int SendAppspawnTimeMessage(const CmdAgent *agent, uint16_t type, const char *ptyName)
{
#ifdef ENABLE_ENTER_APPSPAWN_SANDBOX
    if ((agent == NULL) || (ptyName == NULL)) {
        BEGET_LOGE("Invalid parameter");
        return -1;
    }

    AppSpawnClientHandle clientHandle;
    int ret = AppSpawnClientInit("Appspawn", &clientHandle);
    BEGET_ERROR_CHECK(ret == 0, return -1, "AppSpawnClientInit error, errno = %d", errno);
    AppSpawnReqMsgHandle reqHandle;
    ret = AppSpawnReqMsgCreate(MSG_BEGET_SPAWNTIME, "init", &reqHandle);
    if (ret != 0) {
        AppSpawnClientDestroy(clientHandle);
        return -1;
    }
    AppSpawnResult result = {};
    ret = AppSpawnClientSendMsg(clientHandle, reqHandle, &result);
    if (ret != 0) {
        AppSpawnClientDestroy(clientHandle);
        return -1;
    }
    int minAppspawnTime = result.result;
    int maxAppspawnTime = result.pid;
    printf("minAppspawnTime: %d, maxAppspawnTime: %d \n", minAppspawnTime, maxAppspawnTime);
    AppSpawnClientDestroy(clientHandle);
    return 0;
#endif
    return -1;
}

static int main_cmd(BShellHandle shell, int argc, char* argv[])
{
    if (argc != APPSPAWN_CMD_NUMBER) {
        BShellCmdHelp(shell, argc, argv);
        return 0;
    }
    CmdAgent agent;
    int ret = InitPtyInterface(&agent, ACTION_APP_SPAWNTIME, "init", (CallbackSendMsgProcess)SendAppspawnTimeMessage);
    if (ret != 0) {
        BEGET_LOGE("initptyinterface failed");
        return -1;
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    CmdInfo infos[] = {
        {"appspawn_time", main_cmd, "get appspawn time", "appspawn_time", ""},
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}

