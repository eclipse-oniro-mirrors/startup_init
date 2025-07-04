/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "onremotedied_fuzzer.h"
#include "if_system_ability_manager.h"
#include "init_param.h"
#include "iservice_registry.h"
#include "iwatcher.h"
#include "iwatcher_manager.h"
#include "message_parcel.h"
#include "parameter.h"
#include "param_manager.h"
#include "param_utils.h"
#include "system_ability_definition.h"
#include "watcher.h"
#include "watcher_manager.h"
#include "watcher_manager_proxy.h"
#include "service_watcher.h"
#define private public
#define protected public
#include "watcher_manager_kits.h"
#undef protected
#undef private
using namespace OHOS::init_param;

namespace OHOS {
bool FuzzOnRemoteDied(const uint8_t* data, size_t size)
    {
        int value = 0;
        if (data == nullptr || size < sizeof(int)) {
            return false;
        }
        errno_t err = memcpy_s(&value, sizeof(int), data, sizeof(int));
        if (err != 0) {
            return false;
        }
        sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        WATCHER_CHECK(samgr != nullptr, return false, "Get samgr failed");
        sptr<IRemoteObject> object = samgr->GetSystemAbility(PARAM_WATCHER_DISTRIBUTED_SERVICE_ID);
        WATCHER_CHECK(object != nullptr, return false, "Get watcher manager object from samgr failed");
        OHOS::init_param::WatcherManagerKits &instance = OHOS::init_param::WatcherManagerKits::GetInstance();
        instance.GetService();
        if (instance.GetDeathRecipient() != nullptr) {
            instance.GetDeathRecipient()->OnRemoteDied(object);
        }
        object = samgr->GetSystemAbility(value);
        WATCHER_CHECK(object != nullptr, return false, "Get watcher manager object from samgr failed");
        instance.GetService();
        if (instance.GetDeathRecipient() != nullptr) {
            instance.GetDeathRecipient()->OnRemoteDied(object);
        }
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzOnRemoteDied(data, size);
    return 0;
}