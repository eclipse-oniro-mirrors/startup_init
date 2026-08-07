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

#ifndef OHOS_DEVICE_INFO_FIELD_REGISTRY_H
#define OHOS_DEVICE_INFO_FIELD_REGISTRY_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <cJSON.h>

class FieldRegistry {
public:
    using GetterFunc = std::function<cJSON *()>;

    static FieldRegistry &GetInstance();

    bool HasField(const std::string &name) const;
    cJSON *InvokeField(const std::string &name) const;
    cJSON *InvokeAll() const;
    std::vector<std::string> GetAllFieldNames() const;

private:
    FieldRegistry();
    void Register(const std::string &name, GetterFunc getter);

    struct Entry {
        std::string name;
        GetterFunc getter;
    };
    std::vector<Entry> entries_;
    std::unordered_map<std::string, size_t> index_;
};

#endif  // OHOS_DEVICE_INFO_FIELD_REGISTRY_H
