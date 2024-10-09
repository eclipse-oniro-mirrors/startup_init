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

#include "getservicectrlinfo_fuzzer.h"
#include <string>
#include "init_hook.h"
#include "param_manager.h"

#define NAME_LEN_MAX 96

namespace OHOS {
    bool FuzzGetServiceCtrlInfo(const uint8_t* data, size_t size)
    {
        if (size + sizeof("test") >= NAME_LEN_MAX) {
            return false;
        }

        bool result = false;
        ServiceCtrlInfo *serviceInfo = nullptr;
        GetServiceCtrlInfo("ohos.ctl.stop", "test", &serviceInfo);
        if (serviceInfo != nullptr) {
            free(serviceInfo);
            result = true;
        };

        ServiceCtrlInfo *powerctrl = nullptr;
        GetServiceCtrlInfo("ohos.startup.powerctrl", "reboot,updater", &powerctrl);
        if (powerctrl != nullptr) {
            free(powerctrl);
            result = true;
        };

        ServiceCtrlInfo *ctrlInfo = nullptr;
        std::string str(reinterpret_cast<const char*>(data), size);
        GetServiceCtrlInfo(str.c_str(), "test", &ctrlInfo);
        if (ctrlInfo != nullptr) {
            free(ctrlInfo);
            result = true;
        };

        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzGetServiceCtrlInfo(data, size);
    return 0;
}