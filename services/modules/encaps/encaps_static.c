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
#include <securec.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "bootstage.h"
#include "init_log.h"
#include "init_module_engine.h"
#include "init_service.h"

#define SA_MAIN_PATH ("/system/bin/sa_main")

#define OH_ENCAPS_PROC_TYPE_BASE 0x18
#define OH_ENCAPS_PERMISSION_TYPE_BASE 0x1A
#define OH_ENCAPS_MAGIC 'E'
#define OH_PROC_SYS 3
#define SET_PROC_TYPE_CMD _IOW(OH_ENCAPS_MAGIC, OH_ENCAPS_PROC_TYPE_BASE, uint32_t)
#define SET_KERNEL_PERM_TYPE_CMD _IOW(OH_ENCAPS_MAGIC, OH_ENCAPS_PERMISSION_TYPE_BASE, char *)
#define KERNEL_PERM_LENGTH 1024

static void SetKernelPerm(SERVICE_INFO_CTX *serviceCtx)
{
    int fd = 0;
    int ret = 0;
    int procType = OH_PROC_SYS;

    fd = open("/dev/encaps", O_RDWR);
    if (fd < 0) {
        INIT_LOGI("SetKernelPerm open failed, maybe this device is not supported");
        return;
    }

    ret = ioctl(fd, SET_PROC_TYPE_CMD, &procType);
    if (ret != 0) {
        close(fd);
        INIT_LOGE("SetKernelPerm set flag failed %d", ret);
        return;
    }
    Service *service = GetServiceByName(serviceCtx->serviceName);
    if (service == NULL) {
        close(fd);
        INIT_LOGE("SetKernelPerm get service failed");
        return;
    }
    if (service->kernelPerms != NULL) {
        char *kernelPerms = calloc(1, KERNEL_PERM_LENGTH);
        if (kernelPerms == NULL) {
            INIT_LOGE("SetKernelPerm malloc kernelPerms failed");
            close(fd);
            return;
        }
        ret = strncpy_s(kernelPerms, KERNEL_PERM_LENGTH, service->kernelPerms, KERNEL_PERM_LENGTH - 1);
        if (ret != EOK) {
            INIT_LOGE("SetKernelPerm copy kernelPerms failed");
            close(fd);
            free(kernelPerms);
            return;
        }
        ret = ioctl(fd, SET_KERNEL_PERM_TYPE_CMD, service->kernelPerms);
        if (ret != 0) {
            INIT_LOGE("SetKernelPerm set encaps permission failed");
        }
        free(kernelPerms);
    }
    close(fd);
}

static void SetKernelPermForSa(SERVICE_INFO_CTX *serviceCtx)
{
    if (serviceCtx->reserved == NULL) {
        return;
    }
    if (strncmp(SA_MAIN_PATH, serviceCtx->reserved, strlen(SA_MAIN_PATH)) == 0) {
        SetKernelPerm(serviceCtx);
    }
}

MODULE_CONSTRUCTOR(void)
{
    // Add hook to set encaps
    InitAddServiceHook(SetKernelPermForSa, INIT_SERVICE_SET_PERMS_BEFORE);
}
