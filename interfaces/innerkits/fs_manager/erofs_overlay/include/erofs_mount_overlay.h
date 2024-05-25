/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
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

#ifndef EROFS_MOUNT_OVERLAY_H
#define EROFS_MOUNT_OVERLAY_H

#include "erofs_overlay_common.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

int DoMountOverlayDevice(FstabItem *item);

int MountExt4Device(const char *dev, const char *mnt, bool isFirstMount);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // EROFS_MOUNT_OVERLAY_H
