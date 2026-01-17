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
#ifndef APPSPAWN_EROFS_REMOUNT_OVERLAY_MOCK_H
#define APPSPAWN_EROFS_REMOUNT_OVERLAY_MOCK_H

int MockGetremountresult();

void MockSetremountresultflag();

int MockRemountoverlay();

int MockMountoverlayone(const char *mnt);

void MockOverlayremountvendorpre();

void MockOverlayremountvendorpost();

namespace OHOS {
namespace AppSpawn {
class ErofsRemountOverlay {
public:
    virtual ~ErofsRemountOverlay() = default;
    virtual int GetRemountResult() = 0;

    virtual void SetRemountResultFlag() = 0;

    virtual int RemountOverlay() = 0;

    virtual int MountOverlayOne(const char *mnt) = 0;

    virtual void OverlayRemountVendorPre() = 0;

    virtual void OverlayRemountVendorPost() = 0;

public:
    static inline std::shared_ptr<ErofsRemountOverlay> erofsRemountOverlayFunc = nullptr;
};

class ErofsRemountOverlayMock : public ErofsRemountOverlay {
public:

    MOCK_METHOD(int, GetRemountResult, ());

    MOCK_METHOD(void, SetRemountResultFlag, ());

    MOCK_METHOD(int, RemountOverlay, ());

    MOCK_METHOD(int, MountOverlayOne, (const char *mnt));

    MOCK_METHOD(void, OverlayRemountVendorPre, ());

    MOCK_METHOD(void, OverlayRemountVendorPost, ());

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_EROFS_REMOUNT_OVERLAY_MOCK_H
