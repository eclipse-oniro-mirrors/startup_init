/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "device.h"
#include "fs_manager/fs_manager.h"
#include "init.h"
#include "init_cmds.h"
#include "init_log.h"
#include "securec.h"

int MountRequriedPartitions(void)
{
    const char *fstabFiles[] = {"/etc/fstab.required", NULL};
    int i = 0;
    int rc = -1;
    while (fstabFiles[i] != NULL) {
        if (access(fstabFiles[i], F_OK) == 0) {
            INIT_LOGI("Mount required partition from %s", fstabFiles[i]);
            rc = MountAllWithFstabFile(fstabFiles[i], 1);
        } else {
            INIT_LOGE("Cannot access fstab file \" %s \"", fstabFiles[i]);
        }
        if (rc == 0) {
            break;
        }
        i++;
    }
    return rc;
}