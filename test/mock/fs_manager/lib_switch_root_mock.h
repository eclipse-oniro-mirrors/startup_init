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
#ifndef APPSPAWN_SWITCH_ROOT_MOCK_H
#define APPSPAWN_SWITCH_ROOT_MOCK_H

int MockSwitchroot(const char *newRoot);

namespace OHOS {
namespace AppSpawn {
class SwitchRoot {
public:
    virtual ~SwitchRoot() = default;
    virtual int SwitchRoot(const char *newRoot) = 0;

public:
    static inline std::shared_ptr<SwitchRoot> switchRootFunc = nullptr;
};

class SwitchRootMock : public SwitchRoot {
public:

    MOCK_METHOD(int, SwitchRoot, (const char *newRoot));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_SWITCH_ROOT_MOCK_H
