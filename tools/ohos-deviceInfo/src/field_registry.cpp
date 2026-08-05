/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "field_registry.h"

#include "int_getters.h"
#include "string_getters.h"

namespace {
cJSON *WrapStr(const char *value)
{
    return cJSON_CreateString(value);
}

cJSON *WrapInt(int value)
{
    return cJSON_CreateNumber(value);
}
}  // namespace

FieldRegistry &FieldRegistry::GetInstance()
{
    static FieldRegistry instance;
    return instance;
}

FieldRegistry::FieldRegistry()
{
    Register("osFullName", []() { return WrapStr(GetOSFullNameStr()); });
    Register("osReleaseType", []() { return WrapStr(GetOsReleaseTypeStr()); });
    Register("displayVersion", []() { return WrapStr(GetDisplayVersionStr()); });
    Register("distributionOSName", []() { return WrapStr(GetDistributionOSNameStr()); });
    Register("distributionOSVersion", []() { return WrapStr(GetDistributionOSVersionStr()); });
    Register("distributionOSApiName", []() { return WrapStr(GetDistributionOSApiNameStr()); });
    Register("distributionOSReleaseType", []() { return WrapStr(GetDistributionOSReleaseTypeStr()); });
    Register("sdkApiVersion", []() { return WrapInt(GetSdkApiVersionInt()); });
    Register("sdkMinorApiVersion", []() { return WrapInt(GetSdkMinorApiVersionInt()); });
    Register("sdkPatchApiVersion", []() { return WrapInt(GetSdkPatchApiVersionInt()); });
    Register("distributionOSApiVersion", []() { return WrapInt(GetDistributionOSApiVersionInt()); });
}

void FieldRegistry::Register(const std::string &name, GetterFunc getter)
{
    if (index_.find(name) != index_.end()) {
        return;
    }
    Entry entry;
    entry.name = name;
    entry.getter = std::move(getter);
    index_[name] = entries_.size();
    entries_.push_back(std::move(entry));
}

bool FieldRegistry::HasField(const std::string &name) const
{
    return index_.find(name) != index_.end();
}

cJSON *FieldRegistry::InvokeField(const std::string &name) const
{
    auto it = index_.find(name);
    if (it == index_.end()) {
        return cJSON_CreateNull();
    }
    return entries_[it->second].getter();
}

cJSON *FieldRegistry::InvokeAll() const
{
    cJSON *data = cJSON_CreateObject();
    for (const auto &entry : entries_) {
        cJSON_AddItemToObject(data, entry.name.c_str(), entry.getter());
    }
    return data;
}

std::vector<std::string> FieldRegistry::GetAllFieldNames() const
{
    std::vector<std::string> names;
    names.reserve(entries_.size());
    for (const auto &entry : entries_) {
        names.push_back(entry.name);
    }
    return names;
}
