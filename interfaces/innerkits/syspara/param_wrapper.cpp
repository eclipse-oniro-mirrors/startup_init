/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "param_wrapper.h"

#include <unordered_map>
#include <vector>

#include "beget_ext.h"
#include "param_comm.h"
#include "init_param.h"
#include "init_utils.h"
#include "sysparam_errno.h"
#include "securec.h"
#include "parameter.h"
#include "parameters.h"

namespace OHOS {
namespace system {
static constexpr int MAX_VALUE_LEN = 128;
bool SetParameter(const std::string& key, const std::string& value)
{
    int ret = SystemSetParameter(key.c_str(), value.c_str());
    return (ret == 0) ? true : false;
}

template<typename T>
bool StringToInt(const std::string& str, T min, T max, T& out)
{
    long long int result = 0;
    if (StringToLL(str.c_str(), &result) != 0) {
        return false;
    }
    if (result < min || max < result) {
        return false;
    }
    out = static_cast<T>(result);
    return true;
}

template<typename T>
bool StringToUint(const std::string& str, T max, T& out)
{
    unsigned long long int result = 0;
    if (StringToULL(str.c_str(), &result) != 0) {
        return false;
    }
    if (max < result) {
        return false;
    }
    out = static_cast<T>(result);
    return true;
}

std::string GetParameter(const std::string& key, const std::string& def)
{
    uint32_t size = 0;
    int ret = SystemReadParam(key.c_str(), NULL, &size);
    if (ret == 0) {
        std::vector<char> value(size + 1);
        ret = SystemReadParam(key.c_str(), value.data(), &size);
        if (ret == 0) {
            return std::string(value.data());
        }
    }
    if (IsValidParamValue(def.c_str(), MAX_VALUE_LEN) == 1) {
        return std::string(def);
    }
    return "";
}

bool GetBoolParameter(const std::string& key, bool def)
{
    static const std::string trueMap[] = { "1", "y", "yes", "on", "true" };
    static const std::string falseMap[] = { "0", "off", "n", "no", "false" };
    std::string value = GetParameter(key, "");
    for (size_t i = 0; i < sizeof(trueMap) / sizeof(trueMap[0]); i++) {
        if (trueMap[i] == value) {
            return true;
        }
    }
    for (size_t i = 0; i < sizeof(falseMap) / sizeof(falseMap[0]); i++) {
        if (falseMap[i] == value) {
            return false;
        }
    }
    return def;
}

int GetStringParameter(const std::string &key, std::string &value, const std::string def)
{
    uint32_t size = 0;
    int ret = SystemReadParam(key.c_str(), NULL, &size);
    if (ret == 0) {
        std::vector<char> data(size + 1);
        ret = SystemReadParam(key.c_str(), data.data(), &size);
        if (ret == 0) {
            value = std::string(data.data());
            return EC_SUCCESS;
        }
    }
    if (IsValidParamValue(def.c_str(), MAX_VALUE_LEN) == 1) {
        value = std::string(def);
        return EC_SUCCESS;
    }
    return EC_FAILURE;
}

template<typename T>
T GetIntParameter(const std::string& key, T def, T min, T max)
{
    if (!std::is_signed<T>::value) {
        return def;
    }
    T result;
    std::string value = GetParameter(key, "");
    if (!value.empty() && StringToInt(value, min, max, result)) {
        return result;
    }
    return def;
}

template int8_t GetIntParameter(const std::string&, int8_t, int8_t, int8_t);
template int16_t GetIntParameter(const std::string&, int16_t, int16_t, int16_t);
template int32_t GetIntParameter(const std::string&, int32_t, int32_t, int32_t);
template int64_t GetIntParameter(const std::string&, int64_t, int64_t, int64_t);

template<typename T>
T GetUintParameter(const std::string& key, T def, T max)
{
    if (!std::is_unsigned<T>::value) {
        return def;
    }
    T result;
    std::string value = GetParameter(key, "");
    if (!value.empty() && StringToUint(value, max, result)) {
        return result;
    }
    return def;
}

template uint8_t GetUintParameter(const std::string&, uint8_t, uint8_t);
template uint16_t GetUintParameter(const std::string&, uint16_t, uint16_t);
template uint32_t GetUintParameter(const std::string&, uint32_t, uint32_t);
template uint64_t GetUintParameter(const std::string&, uint64_t, uint64_t);

std::string GetDeviceType(void)
{
    std::unordered_map<std::string, std::string> deviceTypeMap = {
        {"watch", "wearable"},
        {"fitnessWatch", "liteWearable"},
    };
    static const char *productType = nullptr;
    const char *type = GetProperty("const.product.devicetype", &productType);
    if (type == nullptr) {
        type = GetProperty("const.build.characteristics", &productType);
    }
    if (deviceTypeMap.count(type) != 0) {
        return deviceTypeMap[type];
    }
    return std::string(type);
}

int GetIntParameter(const std::string &key, int def)
{
    return GetIntParameter(key, def, INT_MIN, INT_MAX);
}
}  // namespace system
}  // namespace OHOS