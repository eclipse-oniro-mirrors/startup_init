/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include "udid.h"

#ifdef OHOS_LITE
#include "hal_sys_param.h"
#endif
#include "init_param.h"
#include "param_comm.h"
#include "securec.h"
#include "sysparam_errno.h"
#include <stdatomic.h>
#include <limits.h>

INIT_LOCAL_API const char *GetSerial_(void)
{
#ifdef OHOS_LITE
    return HalGetSerial();
#else
    static _Atomic (char *)ohosSerial = NULL;
    if (ohosSerial != NULL) {
        return ohosSerial;
    }

    char *value;
    BEGET_CHECK((value = (char *)calloc(1, PARAM_VALUE_LEN_MAX)) != NULL, return NULL);
    uint32_t len = PARAM_VALUE_LEN_MAX;
    int ret = SystemGetParameter("ohos.boot.sn", value, &len);
    BEGET_CHECK(ret == 0, free(value);
        return NULL);
    if (ohosSerial != NULL) {
        free(value);
        return ohosSerial;
    }
    atomic_store_explicit(&ohosSerial, value, memory_order_release);
    return ohosSerial;
#endif
}

INIT_LOCAL_API int GetUdidFromParam(char *udid, uint32_t size)
{
    uint32_t len = size;
    int ret = SystemGetParameter("const.product.udid", udid, &len);
    BEGET_CHECK(ret != 0, return ret);

    len = size;
    ret = SystemGetParameter("const.product.devUdid", udid, &len);
    BEGET_CHECK(ret != 0, return ret);
    return ret;
}

INIT_LOCAL_API int GetDevUdid_(char *udid, int size)
{
    if (size < UDID_LEN || udid == NULL) {
        return EC_FAILURE;
    }

    int ret = GetUdidFromParam(udid, (uint32_t)size);
    BEGET_CHECK(ret != 0, return ret);

#ifdef OHOS_LITE
    ret = CalcDevUdid(udid, size);
#endif
    return ret;
}

INIT_LOCAL_API int GetDiskSN_(char *diskSN, int size)
{
    if (size < DISK_SN_LEN || diskSN == NULL) {
        return EC_FAILURE;
    }
    char diskSNPath[DISK_SN_PATH_LEN] = {0};
    uint32_t diskSNPathLen = DISK_SN_PATH_LEN;
    if (SystemGetParameter("const.disk.sn.filepath", diskSNPath, &diskSNPathLen) != 0) {
        BEGET_LOGE("const.disk.sn.filepath read failed");
        return EC_FAILURE;
    }
    char realDiskSNPath[PATH_MAX] = {0};
    if (realpath(diskSNPath, realDiskSNPath) == NULL) {
        BEGET_LOGE("GetDiskSN_ path file failed");
        return EC_FAILURE;
    }
    FILE *file = fopen(realDiskSNPath, "r");
    if (file == NULL) {
        BEGET_LOGE("GetDiskSN_ open file failed");
        return EC_FAILURE;
    }
    if (fscanf_s(file, "%s", diskSN, size) <= 0) {
        BEGET_LOGE("GetDiskSN_ read file failed");
        if (fclose(file) != 0) {
            BEGET_LOGE("GetDiskSN_ close file failed");
        }
        return EC_FAILURE;
    }
    if (fclose(file) != 0) {
        BEGET_LOGE("GetDiskSN_ close file failed");
    }
    return 0;
}