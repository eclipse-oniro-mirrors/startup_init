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
#include "processwatchermessage_fuzzer.h"
#include <string>
#include <memory>
#include "init_utils.h"
#include "securec.h"
#define private public
#include "watcher_manager.h"
#undef private
using namespace OHOS::init_param;

namespace OHOS {
bool FuzzProcessWatcherMessage(const uint8_t* data, size_t size)
    {
        constexpr size_t minsize = sizeof(int) + sizeof(uint32_t) + 1;
        if (size < minsize) {
            return false;
        }

        int type = 0;
        if (memcpy_s(&type, sizeof(int), data, sizeof(int)) != 0) {
            return false;
        }

        uint32_t msgSize = 0;
        if (memcpy_s(&msgSize, sizeof(uint32_t), data + sizeof(int), sizeof(uint32_t)) != 0) {
            return false;
        }

        const char* name = reinterpret_cast<const char*>(data + sizeof(int) + sizeof(uint32_t));
        size_t namelen = size - sizeof(int) - sizeof(uint32_t);
        
        std::unique_ptr<char[]> safename = std::make_unique<char[]>(namelen + 1);
        if (memcpy_s(safename.get(), namelen + 1, name, namelen) != 0) {
            return false;
        }
        safename[namelen] = '\0';

        ParamMessage* message = CreateParamMessage(type, safename.get(), msgSize);
        if (message == nullptr) {
            return false;
        }
        std::unique_ptr<WatcherManager> watcherManager = std::make_unique<WatcherManager>(0, true);
        watcherManager->ProcessWatcherMessage(message);
        free(message);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzProcessWatcherMessage(data, size);
    return 0;
}