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

#include <string>
#include <memory>
#include "watcher_manager.h"
#include "RefreshWatcher_fuzzer.h"

using namespace OHOS::init_param;
namespace OHOS {
    bool FuzzRefreshWatcher(const uint8_t* data, size_t size)
    {
        bool result = false;
        std::unique_ptr<WatcherManager> watcherManager = std::make_unique<WatcherManager>(0, true);
        if (watcherManager == nullptr) {
            return result;
        }
        std::string str(reinterpret_cast<const char*>(data), size);

        if (!watcherManager->RefreshWatcher(str, *data)) {
            result = true;
        };
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzRefreshWatcher(data, size);
    return 0;
}