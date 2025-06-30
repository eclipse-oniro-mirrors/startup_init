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

#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <memory>
#include "dump_fuzzer.h"
#include "init_utils.h"
#include "securec.h"
#define protected public
#include "watcher_manager.h"
#undef private
using namespace OHOS::init_param;

namespace OHOS {
    std::vector<std::u16string> ParseFuzzDataToArgs(const uint8_t* data, size_t size)
    {
        std::vector<std::u16string> args;
        if (size == 0) return args;

        std::string input(reinterpret_cast<const char*>(data), size);
        size_t start = 0;
        size_t end = input.find('\n');
        while (end != std::string::npos) {
            std::string token = input.substr(start, end - start);
            if (!token.empty()) {
                args.emplace_back(token.begin(), token.end());
            }
            start = end + 1;
            end = input.find('\n', start);
        }
        if (start < input.size()) {
            std::string token = input.substr(start);
            args.emplace_back(token.begin(), token.end());
        }
        return args;
    }

    bool FuzzAddDUMP(const uint8_t* data, size_t size)
    {
        std::unique_ptr<WatcherManager> watcherManager = std::make_unique<WatcherManager>(0, true);
        int fd = open("/dev/null", O_WRONLY);
        std::vector<std::u16string> args = ParseFuzzDataToArgs(data, size);
        watcherManager->Dump(fd, args);
        close(fd);
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzAddDUMP(data, size);
    return 0;
}