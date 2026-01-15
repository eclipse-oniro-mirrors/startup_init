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
#ifndef APPSPAWN_EROFS_MOUNT_OVERLAY_MOCK_H
#define APPSPAWN_EROFS_MOUNT_OVERLAY_MOCK_H

int MockDomountoverlaydevice(FstabItem *item);

int MockMountext4device(const char *dev, const char *mnt, bool isFirstMount);

namespace OHOS {
namespace AppSpawn {
class ErofsMountOverlay {
public:
    virtual ~ErofsMountOverlay() = default;
    virtual int DoMountOverlayDevice(FstabItem *item) = 0;

    virtual int MountExt4Device(const char *dev, const char *mnt, bool isFirstMount) = 0;

public:
    static inline std::shared_ptr<ErofsMountOverlay> erofsMountOverlayFunc = nullptr;
};

class ErofsMountOverlayMock : public ErofsMountOverlay {
public:

    MOCK_METHOD(int, DoMountOverlayDevice, (FstabItem *item));

    MOCK_METHOD(int, MountExt4Device, (const char *dev, const char *mnt, bool isFirstMount));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_EROFS_MOUNT_OVERLAY_MOCK_H
