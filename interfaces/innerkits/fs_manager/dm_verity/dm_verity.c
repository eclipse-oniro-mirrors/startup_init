/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "dm_verity.h"
#include "fs_hvb.h"
#include "hvb_cmdline.h"
#include "securec.h"
#include "beget_ext.h"
#include <stdbool.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define HVB_VB_STATE_STR_MAX_LEN 32
#define HVB_FORCE_ENABLE_STR_MAX_LEN 16
#define HVB_CMDLINE_HVB_FORCE_ENABLE "ohos.boot.hvb.oem_swtype"

#define DM_VERITY_RETURN_ERR_IF_NULL(__ptr)             \
    do {                                                \
        if ((__ptr) == NULL) {                          \
            BEGET_LOGE("error, %s is NULL\n", #__ptr); \
            return -1;                                  \
        }                                               \
    } while (0)

static bool HvbDmVerityIsEnable(void)
{
    int rc;
    char forceEnable[HVB_FORCE_ENABLE_STR_MAX_LEN] = {0};
    char vBState[HVB_VB_STATE_STR_MAX_LEN] = {0};

    rc = FsHvbGetValueFromCmdLine(&forceEnable[0], sizeof(forceEnable), HVB_CMDLINE_HVB_FORCE_ENABLE);
    if (rc == 0 && strcmp(&forceEnable[0], "factory") == 0) {
        return true;
    }

    rc = FsHvbGetValueFromCmdLine(&vBState[0], sizeof(vBState), HVB_CMDLINE_VB_STATE);

    if (rc != 0) {
        BEGET_LOGE("error 0x%x, get verifed boot state", rc);
        return false;
    }

    if (strcmp(&vBState[0], "false") == 0 || strcmp(&vBState[0], "FALSE") == 0) {
        return false;
    }

    return true;
}

int HvbDmVerityinit(const Fstab *fstab)
{
    int rc;
    FstabItem *p = NULL;

    if (!HvbDmVerityIsEnable()) {
        BEGET_LOGI("hvb not enable, not init");
        return 0;
    }

    for (p = fstab->head; p != NULL; p = p->next) {
        if (p->fsManagerFlags & FS_MANAGER_HVB)
            break;
    }

    if (p == NULL) {
        BEGET_LOGI("no need init fs hvb");
        return 0;
    }

    rc = FsHvbInit();
    if (rc != 0) {
        BEGET_LOGE("init fs hvb error, ret=%d", rc);
        return rc;
    }

    return rc;
}

int HvbDmVeritySetUp(FstabItem *fsItem)
{
    int rc;

    if (!HvbDmVerityIsEnable()) {
        BEGET_LOGI("hvb not enable, not setup");
        return 0;
    }

    DM_VERITY_RETURN_ERR_IF_NULL(fsItem);

    if ((fsItem->fsManagerFlags & FS_MANAGER_HVB) == 0) {
        BEGET_LOGW("device %s not need hvb", fsItem->deviceName ? fsItem->deviceName : "none");
        return 0;
    }

    rc = FsHvbSetupHashtree(fsItem);
    if (rc != 0) {
        BEGET_LOGE("error, setup hashtree fail, ret=%d", rc);
        return rc;
    }

    return rc;
}

void HvbDmVerityFinal(void)
{
    int rc;

    if (!HvbDmVerityIsEnable()) {
        BEGET_LOGI("hvb not enable, not final");
        return;
    }

    rc = FsHvbFinal();
    if (rc != 0) {
        BEGET_LOGE("final fs hvb error, ret=%d", rc);
        return;
    }
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
