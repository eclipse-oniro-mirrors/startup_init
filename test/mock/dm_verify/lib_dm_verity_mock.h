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
#ifndef APPSPAWN_DM_VERITY_MOCK_H
#define APPSPAWN_DM_VERITY_MOCK_H

int MockHvbdmverityinit(const Fstab *fstab);

int MockHvbdmveritysetup(FstabItem *fsItem);

void MockHvbdmverityfinal();

namespace OHOS {
namespace AppSpawn {
class DmVerity {
public:
    virtual ~DmVerity() = default;
    virtual int HvbDmVerityinit(const Fstab *fstab) = 0;

    virtual int HvbDmVeritySetUp(FstabItem *fsItem) = 0;

    virtual void HvbDmVerityFinal() = 0;

public:
    static inline std::shared_ptr<DmVerity> dmVerityFunc = nullptr;
};

class DmVerityMock : public DmVerity {
public:

    MOCK_METHOD(int, HvbDmVerityinit, (const Fstab *fstab));

    MOCK_METHOD(int, HvbDmVeritySetUp, (FstabItem *fsItem));

    MOCK_METHOD(void, HvbDmVerityFinal, ());

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_DM_VERITY_MOCK_H
