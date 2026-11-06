/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <memory>
#include <mutex>
#include <string>
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
    int32_t GetCpuId(std::string& result) override;
    int32_t GetCpuArchitecture(std::string& result) override;
    int32_t GetCpuVendor(std::string& result) override;
    int32_t GetBoardSerial(std::string& result) override;
    int32_t GetBoardVendor(std::string& result) override;
    int32_t GetBoardProductName(std::string& result) override;
    int32_t GetBiosVendor(std::string& result) override;
    int32_t GetBiosVersion(std::string& result) override;
    int32_t GetBiosReleaseDate(std::string& result) override;
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
private:
    void ClearBoardInfoCache();
    // HDI boardInfo caches (cleared on OnStop to avoid stale values after SA reload).
    // Protected by boardInfoMutex_; shared_ptr ensures readers never dereference freed memory.
    std::mutex boardInfoMutex_;
    std::shared_ptr<std::string> boardSerialCache_;
    std::shared_ptr<std::string> cpuIdCache_;
    std::shared_ptr<std::string> cpuArchCache_;
    std::shared_ptr<std::string> cpuVendorCache_;
    std::shared_ptr<std::string> boardVendorCache_;
    std::shared_ptr<std::string> boardProductNameCache_;
    std::shared_ptr<std::string> biosVendorCache_;
    std::shared_ptr<std::string> biosVersionCache_;
    std::shared_ptr<std::string> biosReleaseDateCache_;
#ifdef STARTUP_INIT_TEST
public:
    void TestInjectBoardInfoCache(const std::string &boardSerial, const std::string &cpuId,
        const std::string &cpuArch, const std::string &cpuVendor, const std::string &boardVendor,
        const std::string &boardProductName, const std::string &biosVendor,
        const std::string &biosVersion, const std::string &biosReleaseDate)
    {
        std::lock_guard<std::mutex> lock(boardInfoMutex_);
        boardSerialCache_ = std::make_shared<std::string>(boardSerial);
        cpuIdCache_ = std::make_shared<std::string>(cpuId);
        cpuArchCache_ = std::make_shared<std::string>(cpuArch);
        cpuVendorCache_ = std::make_shared<std::string>(cpuVendor);
        boardVendorCache_ = std::make_shared<std::string>(boardVendor);
        boardProductNameCache_ = std::make_shared<std::string>(boardProductName);
        biosVendorCache_ = std::make_shared<std::string>(biosVendor);
        biosVersionCache_ = std::make_shared<std::string>(biosVersion);
        biosReleaseDateCache_ = std::make_shared<std::string>(biosReleaseDate);
    }
#endif
};

} // namespace device_info
} // namespace OHOS
#endif // DEVICE_INFO_SERVICE_H