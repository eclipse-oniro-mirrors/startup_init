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

#include "param_fuzzer.h"
#include <string>
#include "parameter.h"
#include "fs_manager/fs_manager.h"
#include "sysversion.h"


namespace OHOS {
    bool FuzzParam(const uint8_t* data, size_t size)
    {
        std::string str(reinterpret_cast<const char*>(data), size);
        WaitParameter(str.c_str(), str.c_str(), 1);
        GetBootSlots();
        GetFirstApiVersion();
        GetSerial();
        GetProductModel();
        GetIncrementalVersion();
        GetHardwareModel();
        GetBuildVersion();
        LoadFstabFromCommandLine();
        GetDeviceType();
        GetProductSeries();
        SaveParameters();
        GetDisplayVersion();
        GetHardwareProfile();
        GetBrand();
        GetBuildHost();
        GetVersionId();
        GetSoftwareModel();
        GetMarketName();
        GetSystemCommitId();
        return true;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzParam(data, size);
    return 0;
}