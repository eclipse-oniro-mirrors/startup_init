/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#ifndef APPSPAWN_PARAM_WRAPPER_MOCK_H
#define APPSPAWN_PARAM_WRAPPER_MOCK_H

int MockGetstringparameter(const std::string &key, std::string &value, const std::string def = "");

int MockGetintparameter(const std::string &key, int def);

namespace OHOS {
namespace AppSpawn {
class ParamWrapper {
public:
    virtual ~ParamWrapper() = default;
    virtual int GetStringParameter(const std::string &key, std::string &value, const std::string def = "") = 0;

    virtual int GetIntParameter(const std::string &key, int def) = 0;

public:
    static inline std::shared_ptr<ParamWrapper> paramWrapperFunc = nullptr;
};

class ParamWrapperMock : public ParamWrapper {
public:

    MOCK_METHOD(int, GetStringParameter, (const std::string &key, std::string &value, const std::string def = ""));

    MOCK_METHOD(int, GetIntParameter, (const std::string &key, int def));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_PARAM_WRAPPER_MOCK_H
