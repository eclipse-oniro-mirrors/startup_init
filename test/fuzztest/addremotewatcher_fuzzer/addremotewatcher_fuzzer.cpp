/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "watcher.h"
#include "watcher_proxy.h"
#include "addremotewatcher_fuzzer.h"
#include <string>
#include <memory>
#include "watcher_utils.h"
#define protected public
#include "watcher_manager.h"
#undef protected
using namespace OHOS::init_param;

class FuzzWatcher final : public Watcher {
public:
    FuzzWatcher() {}
    ~FuzzWatcher() = default;
    int32_t OnParameterChange(const std::string &prefix, const std::string &name, const std::string &value) override
    {
        return 0;
    }
};
namespace OHOS {
    bool FuzzAddRemoteWatcher(const uint8_t* data, size_t size)
    {
        bool result = false;
        sptr<Watcher> watcher = new FuzzWatcher();
        std::unique_ptr<WatcherManager> watcherManager = std::make_unique<WatcherManager>(0, true);
        uint32_t id = static_cast<const uint32_t>(*data);
        uint32_t watcherId = 0;
        watcherManager->OnStart();
        if (!watcherManager->AddRemoteWatcher(id, watcherId, watcher)) {
            result = true;
            watcherManager->DelRemoteWatcher(watcherId);
        };
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzAddRemoteWatcher(data, size);
    return 0;
}