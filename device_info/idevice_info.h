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

#ifndef OHOS_SYSTEM_IDEVICEID_H
#define OHOS_SYSTEM_IDEVICEID_H

#include <iostream>
#include "beget_ext.h"
#include "iremote_broker.h"
#include "iremote_proxy.h"

namespace OHOS {
namespace device_info {
class IDeviceInfo : public OHOS::IRemoteBroker {
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"ohos.system.IDeviceInfo");

    virtual int32_t GetUdid(std::string& result) = 0;
    virtual int32_t GetSerialID(std::string& result) = 0;

    static constexpr int COMMAND_GET_UDID = MIN_TRANSACTION_ID + 0;
    static constexpr int COMMAND_GET_SERIAL_ID = MIN_TRANSACTION_ID + 1;
    static constexpr int ERR_FAIL = -1;
    const std::string PERMISSION_UDID = "ohos.permission.sec.ACCESS_UDID";
};
} // namespace device_info
} // namespace OHOS

#ifndef DINFO_DOMAIN
#define DINFO_DOMAIN (BASE_DOMAIN + 8)
#endif

#define DINFO_LOGI(fmt, ...) STARTUP_LOGI(DINFO_DOMAIN, "DeviceInfoKits", fmt, ##__VA_ARGS__)
#define DINFO_LOGE(fmt, ...) STARTUP_LOGE(DINFO_DOMAIN, "DeviceInfoKits", fmt, ##__VA_ARGS__)
#define DINFO_LOGV(fmt, ...) STARTUP_LOGV(DINFO_DOMAIN, "DeviceInfoKits", fmt, ##__VA_ARGS__)

#define DINFO_CHECK(ret, exper, ...) \
    if (!(ret)) { \
        DINFO_LOGE(__VA_ARGS__); \
        exper; \
    }
#define DINFO_ONLY_CHECK(ret, exper, ...) \
    if (!(ret)) { \
        exper; \
    }

#endif // OHOS_SYSTEM_IDEVICEID_H