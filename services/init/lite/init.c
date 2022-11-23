/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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
#include "init.h"

#include <sys/sysmacros.h>
#include <unistd.h>
#ifdef __LINUX__
#include <linux/major.h>
#endif
#include "init_group_manager.h"
#include "init_jobs_internal.h"
#include "init_log.h"
#include "init_utils.h"
#ifndef __LINUX__
#include "init_stage.h"
#endif
#include "init_param.h"
#include "loop_event.h"

void SystemInit(void)
{
    SignalInit();
    (void)MakeDirRecursive("/dev/unix/socket", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

void LogInit(void)
{
#ifdef __LINUX__
    int ret = mknod("/dev/kmsg", S_IFCHR | S_IWUSR | S_IRUSR, makedev(MEM_MAJOR, 11));
    if (ret == 0) {
        OpenLogDevice();
    }
#endif
    return;
}

void SystemPrepare(void)
{
}

void ParseInitCfgByPriority(void)
{
    ReadFileInDir(OTHER_CFG_PATH, ".cfg", ParseInitCfg, NULL);
    ReadFileInDir("/vendor/etc/init", ".cfg", ParseInitCfg, NULL);
}

void SystemConfig(void)
{
    InitServiceSpace();
#ifdef LITEOS_SUPPORT
    InitParamService();
    // parse parameters
    (void)LoadDefaultParams("/system/etc/param/ohos_const", LOAD_PARAM_NORMAL);
    (void)LoadDefaultParams("/vendor/etc/param", LOAD_PARAM_NORMAL);
    (void)LoadDefaultParams("/system/etc/param", LOAD_PARAM_ONLY_ADD);

    (void)LoadPersistParams();
#endif
    // read config
    ReadConfig();

    // execute init
    DoJob("pre-init");
#ifndef __LINUX__
    TriggerStage(EVENT1, EVENT1_WAITTIME, QS_STAGE1);
#endif

    DoJob("init");
#ifndef __LINUX__
    TriggerStage(EVENT2, EVENT2_WAITTIME, QS_STAGE2);
#endif

    DoJob("post-init");
#ifndef __LINUX__
    TriggerStage(EVENT3, EVENT3_WAITTIME, QS_STAGE3);
    (void)InitStageFinished();
#endif
    ReleaseAllJobs();
}

void SystemRun(void)
{
#ifndef __LITEOS_A__
    LE_RunLoop(LE_GetDefaultLoop());
#else
    while (1) {
        // pause only returns when a signal was caught and the signal-catching function returned.
        // pause only returns -1, no need to process the return value.
        (void)pause();
    }
#endif
}
