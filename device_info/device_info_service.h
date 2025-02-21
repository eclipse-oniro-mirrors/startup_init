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

#ifndef DEVICE_INFO_SERVICE_H
#define DEVICE_INFO_SERVICE_H

#include <atomic>
#include <mutex>
#include "beget_ext.h"
#include "iremote_stub.h"
#include "system_ability.h"
#include "device_info_stub.h"

namespace OHOS {
namespace device_info {

class DeviceInfoService : public SystemAbility, public DeviceInfoStub {
public:
    DECLARE_SYSTEM_ABILITY(DeviceInfoService);
    DISALLOW_COPY_AND_MOVE(DeviceInfoService);
    explicit DeviceInfoService(int32_t systemAbilityId, bool runOnCreate = true)
        : SystemAbility(systemAbilityId, runOnCreate)
    {
    }
    ~DeviceInfoService() override {}

    int32_t GetUdid(std::string& result) override;
    int32_t GetSerialID(std::string& result) override;
    int32_t GetDiskSN(std::string& result) override;
    int32_t CallbackEnter(uint32_t code) override;
    int32_t CallbackExit(uint32_t code, int32_t result) override;

    static constexpr int ERR_FAIL = -1;
protected:
    bool CheckPermission(const std::string &permission);
    void OnStart(void) override;
    void OnStop(void) override;
    int Dump(int fd, const std::vector<std::u16string>& args) override;
    void ThreadForUnloadSa(void);
    std::atomic_bool threadStarted_ { false };
};

} // namespace device_info
} // namespace OHOS
#endif // DEVICE_INFO_SERVICE_H