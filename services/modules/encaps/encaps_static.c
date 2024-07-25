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
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "bootstage.h"
#include "init_log.h"
#include "init_module_engine.h"

#define SA_MAIN_PATH ("/system/bin/sa_main")

#define OH_ENCAPS_PROC_TYPE_BASE 0x18
#define OH_ENCAPS_MAGIC 'E'
#define OH_PROC_SYS 3
#define SET_PROC_TYPE_CMD _IOW(OH_ENCAPS_MAGIC, OH_ENCAPS_PROC_TYPE_BASE, uint32_t)

static void SetEncapsFlag(uint32_t flag)
{
    int fd = 0;
    int ret = 0;

    fd = open("/dev/encaps", O_RDWR);
    if (fd < 0) {
        INIT_LOGI("SetEncapsFlag open failed, maybe this device is not supported");
        return;
    }

    ret = ioctl(fd, SET_PROC_TYPE_CMD, &flag);
    if (ret != 0) {
        close(fd);
        INIT_LOGE("SetEncapsFlag ioctl failed");
        return;
    }

    close(fd);
}

static void SetEncapsProcType(SERVICE_INFO_CTX *serviceCtx)
{
    if (serviceCtx->reserved == NULL) {
        return;
    }
    if (strncmp(SA_MAIN_PATH, serviceCtx->reserved, strlen(SA_MAIN_PATH)) == 0) {
        SetEncapsFlag(OH_PROC_SYS);
    }
}

MODULE_CONSTRUCTOR(void)
{
    // Add hook to set encaps flag
    InitAddServiceHook(SetEncapsProcType, INIT_SERVICE_SET_PERMS_BEFORE);
}
