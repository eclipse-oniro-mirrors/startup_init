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

#ifndef EROFS_REMOUNT_OVERLAY_H
#define EROFS_REMOUNT_OVERLAY_H

#include "erofs_overlay_common.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define MODEM_DRIVER_MNT_PATH STARTUP_INIT_UT_PATH"/vendor/modem/modem_driver"
#define MODEM_VENDOR_MNT_PATH STARTUP_INIT_UT_PATH"/vendor/modem/modem_vendor"
#define MODEM_FW_MNT_PATH STARTUP_INIT_UT_PATH"/vendor/modem/modem_fw"
#define MODEM_DRIVER_EXCHANGE_PATH STARTUP_INIT_UT_PATH"/mnt/driver_exchange"
#define MODEM_VENDOR_EXCHANGE_PATH STARTUP_INIT_UT_PATH"/mnt/vendor_exchange"
#define MODEM_FW_EXCHANGE_PATH STARTUP_INIT_UT_PATH"/mnt/fw_exchange"
#define REMOUNT_RESULT_PATH STARTUP_INIT_UT_PATH"/data/service/el1/startup/remount/"
#define REMOUNT_RESULT_FLAG STARTUP_INIT_UT_PATH"/data/service/el1/startup/remount/remount.result.done"

#define REMOUNT_NONE 0
#define REMOUNT_SUCC 1
#define REMOUNT_FAIL 2

int GetRemountResult(void);

void SetRemountResultFlag(bool result);

int RemountOverlay(void);

int MountOverlayOne(const char *mnt);

void OverlayRemountVendorPre(void);

void OverlayRemountVendorPost(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif // EROFS_REMOUNT_OVERLAY_H
