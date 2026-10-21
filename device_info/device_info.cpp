/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "beget_ext.h"
#ifdef PARAM_FEATURE_DEVICEINFO
#include "device_info_kits.h"
#endif
#include "param_comm.h"
#include "securec.h"
#include "sysparam_errno.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

int AclGetDevUdid(char *udid, int size)
{
    if (udid == nullptr || size < UDID_LEN) {
        return SYSPARAM_INVALID_INPUT;
    }
    (void)memset_s(udid, size, 0, size);
#ifdef PARAM_FEATURE_DEVICEINFO
    std::string result = {};
    OHOS::device_info::DeviceInfoKits &instance = OHOS::device_info::DeviceInfoKits::GetInstance();
    int ret = instance.GetUdid(result);
    if (ret == 0) {
        ret = strcpy_s(udid, size, result.c_str());
    }
#else
    int ret = GetDevUdid_(udid, size);
#endif
    return ret;
}

const char *AclGetSerial(void)
{
    static char serialNumber[MAX_SERIAL_LEN] = {0};
#ifdef PARAM_FEATURE_DEVICEINFO
    std::string result = {};
    OHOS::device_info::DeviceInfoKits &instance = OHOS::device_info::DeviceInfoKits::GetInstance();
    int ret = instance.GetSerialID(result);
    if (ret == 0) {
        ret = strcpy_s(serialNumber, sizeof(serialNumber), result.c_str());
        BEGET_ERROR_CHECK(ret == 0, return nullptr, "failed copy");
    }
#else
    const char *tmpSerial = GetSerial_();
    if (tmpSerial != nullptr) {
        int ret = strcpy_s(serialNumber, sizeof(serialNumber), tmpSerial);
        BEGET_ERROR_CHECK(ret == 0, return nullptr, "failed copy");
    }
#endif
    return serialNumber;
}

int AclGetDiskSN(char *diskSN, int size)
{
    if (diskSN == nullptr || size < DISK_SN_LEN) {
        return SYSPARAM_INVALID_INPUT;
    }
    (void)memset_s(diskSN, size, 0, size);
    int ret = 0;
#ifdef PARAM_FEATURE_GET_DEVICE_SN
    std::string result = {};
    OHOS::device_info::DeviceInfoKits &instance = OHOS::device_info::DeviceInfoKits::GetInstance();
    ret = instance.GetDiskSN(result);
    if (ret == 0) {
        ret = strcpy_s(diskSN, size, result.c_str());
    }
#endif
    return ret;
}

#define ACL_BOARDINFO_LEN 256
#define ACL_LOGV(fmt, ...) STARTUP_LOGV(BASE_DOMAIN + 8, "BOARDINFO_ACL", fmt, ##__VA_ARGS__)

const char *AclGetCpuId(void)
{
#ifdef PARAM_FEATURE_DEVICEINFO
    static char buf[ACL_BOARDINFO_LEN] = {0};
    std::string result;
    (void)OHOS::device_info::DeviceInfoKits::GetInstance().GetCpuId(result);
    if (strcpy_s(buf, sizeof(buf), result.c_str()) != 0) {
        buf[0] = '\0';
    }
    ACL_LOGV("Acl boardinfo feature=ON");
    return buf;
#else
    ACL_LOGV("Acl boardinfo feature=OFF");
    return "";
#endif
}

const char *AclGetCpuArchitecture(void)
{
#ifdef PARAM_FEATURE_DEVICEINFO
    static char buf[ACL_BOARDINFO_LEN] = {0};
    std::string result;
    (void)OHOS::device_info::DeviceInfoKits::GetInstance().GetCpuArchitecture(result);
    if (strcpy_s(buf, sizeof(buf), result.c_str()) != 0) {
        buf[0] = '\0';
    }
    ACL_LOGV("Acl boardinfo feature=ON");
    return buf;
#else
    ACL_LOGV("Acl boardinfo feature=OFF");
    return "";
#endif
}

const char *AclGetCpuVendor(void)
{
#ifdef PARAM_FEATURE_DEVICEINFO
    static char buf[ACL_BOARDINFO_LEN] = {0};
    std::string result;
    (void)OHOS::device_info::DeviceInfoKits::GetInstance().GetCpuVendor(result);
    if (strcpy_s(buf, sizeof(buf), result.c_str()) != 0) {
        buf[0] = '\0';
    }
    ACL_LOGV("Acl boardinfo feature=ON");
    return buf;
#else
    ACL_LOGV("Acl boardinfo feature=OFF");
    return "";
#endif
}

int AclGetBoardSerial(char *value, int size)
{
    if (value == nullptr || size <= 0) {
        return SYSPARAM_INVALID_INPUT;
    }
    (void)memset_s(value, size, 0, size);
#ifdef PARAM_FEATURE_DEVICEINFO
    std::string result;
    int ret = OHOS::device_info::DeviceInfoKits::GetInstance().GetBoardSerial(result);
    if (ret == 0) {
        DINFO_CHECK(strcpy_s(value, size, result.c_str()) == 0, return SYSPARAM_INVALID_INPUT,
            "AclGetBoardSerial strcpy_s failed");
    }
    ACL_LOGV("AclGetBoardSerial ret=%d", ret);
    return ret;
#else
    ACL_LOGV("Acl boardinfo feature=OFF");
    return 0;
#endif
}

const char *AclGetBoardVendor(void)
{
#ifdef PARAM_FEATURE_DEVICEINFO
    static char buf[ACL_BOARDINFO_LEN] = {0};
    std::string result;
    (void)OHOS::device_info::DeviceInfoKits::GetInstance().GetBoardVendor(result);
    if (strcpy_s(buf, sizeof(buf), result.c_str()) != 0) {
        buf[0] = '\0';
    }
    ACL_LOGV("Acl boardinfo feature=ON");
    return buf;
#else
    ACL_LOGV("Acl boardinfo feature=OFF");
    return "";
#endif
}

const char *AclGetBoardProductName(void)
{
#ifdef PARAM_FEATURE_DEVICEINFO
    static char buf[ACL_BOARDINFO_LEN] = {0};
    std::string result;
    (void)OHOS::device_info::DeviceInfoKits::GetInstance().GetBoardProductName(result);
    if (strcpy_s(buf, sizeof(buf), result.c_str()) != 0) {
        buf[0] = '\0';
    }
    ACL_LOGV("Acl boardinfo feature=ON");
    return buf;
#else
    ACL_LOGV("Acl boardinfo feature=OFF");
    return "";
#endif
}

const char *AclGetBiosVendor(void)
{
#ifdef PARAM_FEATURE_DEVICEINFO
    static char buf[ACL_BOARDINFO_LEN] = {0};
    std::string result;
    (void)OHOS::device_info::DeviceInfoKits::GetInstance().GetBiosVendor(result);
    if (strcpy_s(buf, sizeof(buf), result.c_str()) != 0) {
        buf[0] = '\0';
    }
    ACL_LOGV("Acl boardinfo feature=ON");
    return buf;
#else
    ACL_LOGV("Acl boardinfo feature=OFF");
    return "";
#endif
}

const char *AclGetBiosVersion(void)
{
#ifdef PARAM_FEATURE_DEVICEINFO
    static char buf[ACL_BOARDINFO_LEN] = {0};
    std::string result;
    (void)OHOS::device_info::DeviceInfoKits::GetInstance().GetBiosVersion(result);
    if (strcpy_s(buf, sizeof(buf), result.c_str()) != 0) {
        buf[0] = '\0';
    }
    ACL_LOGV("Acl boardinfo feature=ON");
    return buf;
#else
    ACL_LOGV("Acl boardinfo feature=OFF");
    return "";
#endif
}

const char *AclGetBiosReleaseDate(void)
{
#ifdef PARAM_FEATURE_DEVICEINFO
    static char buf[ACL_BOARDINFO_LEN] = {0};
    std::string result;
    (void)OHOS::device_info::DeviceInfoKits::GetInstance().GetBiosReleaseDate(result);
    if (strcpy_s(buf, sizeof(buf), result.c_str()) != 0) {
        buf[0] = '\0';
    }
    ACL_LOGV("Acl boardinfo feature=ON");
    return buf;
#else
    ACL_LOGV("Acl boardinfo feature=OFF");
    return "";
#endif
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
