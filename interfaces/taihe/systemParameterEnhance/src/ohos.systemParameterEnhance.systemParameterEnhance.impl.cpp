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
#include "ohos.systemParameterEnhance.systemParameterEnhance.proj.hpp"
#include "ohos.systemParameterEnhance.systemParameterEnhance.impl.hpp"
#include "taihe/runtime.hpp"
#include "stdexcept"

#include "beget_ext.h"
#include "parameter.h"
#include "sysparam_errno.h"

static constexpr int MAX_VALUE_LENGTH = PARAM_CONST_VALUE_LEN_MAX;

using namespace taihe;
// using namespace ohos::systemParameterEnhance::systemParameterEnhance;

namespace {
// To be implemented.
string GetParam(std::string key, std::string def)
{
    if (key.empty()) {
        taihe::set_business_error(SYSPARAM_INVALID_INPUT, "input is invalid");
    }
    char value[MAX_VALUE_LENGTH] = {0};
    int result = GetParameter(key.c_str(), def.c_str(), value, MAX_VALUE_LENGTH);
    if (result < 0) {
        taihe::set_business_error(result, "getSync failed");
    }
    return value;
}

string getSync(string_view key, optional_view<string> def)
{
    string defValue = def.value_or("");
    return GetParam(std::string(key), std::string(defValue));
}

string getParam(string_view key, optional_view<string> def)
{
    string defValue = def.value_or("");
    return GetParam(std::string(key), std::string(defValue));
}

string getParamNodef(string_view key)
{
    return GetParam(std::string(key), nullptr);
}

void SetParam(std::string key, std::string value)
{
    if (key.empty() || value.empty()) {
        taihe::set_business_error(SYSPARAM_INVALID_INPUT, "input is invalid");
    }
    if (value.size() >= MAX_VALUE_LENGTH) {
        taihe::set_business_error(SYSPARAM_INVALID_VALUE, "input value is invalid");
    }
    int result = SetParameter(key.c_str(), value.c_str());
    if (result < 0) {
        taihe::set_business_error(result, "set failed");
    }
    return;
}
void setSync(string_view key, string_view value)
{
    SetParam(std::string(key), std::string(value));
}

void setParam(string_view key, string_view value)
{
    SetParam(std::string(key), std::string(value));
}
}  // namespace

TH_EXPORT_CPP_API_getSync(getSync);
TH_EXPORT_CPP_API_getParam(getParam);
TH_EXPORT_CPP_API_getParamNodef(getParamNodef);
TH_EXPORT_CPP_API_setSync(setSync);
TH_EXPORT_CPP_API_setParam(setParam);
