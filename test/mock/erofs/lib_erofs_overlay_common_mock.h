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
#ifndef APPSPAWN_EROFS_OVERLAY_COMMON_MOCK_H
#define APPSPAWN_EROFS_OVERLAY_COMMON_MOCK_H

bool MockIsoverlayenable();

bool MockCheckisext4(const char *dev, uint64_t offset);

bool MockCheckiserofs(const char *dev);

namespace OHOS {
namespace AppSpawn {
class ErofsOverlayCommon {
public:
    virtual ~ErofsOverlayCommon() = default;
    virtual bool IsOverlayEnable() = 0;

    virtual bool CheckIsExt4(const char *dev, uint64_t offset) = 0;

    virtual bool CheckIsErofs(const char *dev) = 0;

public:
    static inline std::shared_ptr<ErofsOverlayCommon> erofsOverlayCommonFunc = nullptr;
};

class ErofsOverlayCommonMock : public ErofsOverlayCommon {
public:

    MOCK_METHOD(bool, IsOverlayEnable, ());

    MOCK_METHOD(bool, CheckIsExt4, (const char *dev, uint64_t offset));

    MOCK_METHOD(bool, CheckIsErofs, (const char *dev));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_EROFS_OVERLAY_COMMON_MOCK_H
