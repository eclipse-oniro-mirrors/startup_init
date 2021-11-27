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

#include "fs_manager/fs_manager.h"
#include "init_cmds.h"
#include "init_log.h"
#include "securec.h"

int MountRequriedPartitions(const Fstab *fstab)
{
    INIT_ERROR_CHECK(fstab != NULL, return -1, "Failed fstab is NULL");
    int rc = -1;
    INIT_LOGI("Mount required partitions");
    rc = MountAllWithFstab(fstab, 1);
    return rc;
}