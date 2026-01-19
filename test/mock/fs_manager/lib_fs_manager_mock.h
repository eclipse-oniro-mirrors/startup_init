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
#ifndef APPSPAWN_FS_MANAGER_MOCK_H
#define APPSPAWN_FS_MANAGER_MOCK_H

Fstab* MockLoadfstabfromcommandline();

int MockGetbootslots();

int MockGetcurrentslot();

void MockReleasefstab(Fstab *fstab);

FstabItem* MockFindfstabitemformountpoint(Fstab fstab, const char *mp);

int MockParsefstabperline(char *str, Fstab *fstab, bool procMounts, const char *separator);

int MockGetblockdevicebymountpoint(const char *mountPoint, const Fstab *fstab, char *deviceName, int nameLen);

int MockGetblockdevicebyname(const char *deviceName, const Fstab *fstab, char* miscDev, size_t size);

bool MockIssupportedfilesystem(const char *fsType);

int MockDoformat(const char *devPath, const char *fsType);

int MockMountoneitem(FstabItem *item);

MountStatus MockGetmountstatusformountpoint(const char *mp);

int MockMountallwithfstabfile(const char *fstabFile, bool required);

int MockMountallwithfstab(const Fstab *fstab, bool required);

int MockUmountallwithfstabfile(const char *file);

int MockMountonewithfstabfile(const char *fstabFile, const char *devName, bool required);

int MockFsmanagerdmremovedevice(const char *devName);

int MockGetblockdevicepath(const char *partName, char *path, size_t size);

int MockUpdateuserdatamedevice(FstabItem *item);

uint64_t MockLookuperofsend(const char *dev);

void MockSetremountflag(bool flag);

bool MockGetremountflag();

int MockLoadfscryptpolicy(char *buf, size_t size);

namespace OHOS {
namespace AppSpawn {
class FsManager {
public:
    virtual ~FsManager() = default;
    virtual Fstab* LoadFstabFromCommandLine() = 0;

    virtual int GetBootSlots() = 0;

    virtual int GetCurrentSlot() = 0;

    virtual void ReleaseFstab(Fstab *fstab) = 0;

    virtual FstabItem* FindFstabItemForMountPoint(Fstab fstab, const char *mp) = 0;

    virtual int ParseFstabPerLine(char *str, Fstab *fstab, bool procMounts, const char *separator) = 0;

    virtual int GetBlockDeviceByMountPoint(const char *mountPoint, const Fstab *fstab, char *deviceName, int nameLen) = 0;

    virtual int GetBlockDeviceByName(const char *deviceName, const Fstab *fstab, char* miscDev, size_t size) = 0;

    virtual bool IsSupportedFilesystem(const char *fsType) = 0;

    virtual int DoFormat(const char *devPath, const char *fsType) = 0;

    virtual int MountOneItem(FstabItem *item) = 0;

    virtual MountStatus GetMountStatusForMountPoint(const char *mp) = 0;

    virtual int MountAllWithFstabFile(const char *fstabFile, bool required) = 0;

    virtual int MountAllWithFstab(const Fstab *fstab, bool required) = 0;

    virtual int UmountAllWithFstabFile(const char *file) = 0;

    virtual int MountOneWithFstabFile(const char *fstabFile, const char *devName, bool required) = 0;

    virtual int FsManagerDmRemoveDevice(const char *devName) = 0;

    virtual int GetBlockDevicePath(const char *partName, char *path, size_t size) = 0;

    virtual int UpdateUserDataMEDevice(FstabItem *item) = 0;

    virtual uint64_t LookupErofsEnd(const char *dev) = 0;

    virtual void SetRemountFlag(bool flag) = 0;

    virtual bool GetRemountFlag() = 0;

    virtual int LoadFscryptPolicy(char *buf, size_t size) = 0;

public:
    static inline std::shared_ptr<FsManager> fsManagerFunc = nullptr;
};

class FsManagerMock : public FsManager {
public:

    MOCK_METHOD(Fstab*, LoadFstabFromCommandLine, ());

    MOCK_METHOD(int, GetBootSlots, ());

    MOCK_METHOD(int, GetCurrentSlot, ());

    MOCK_METHOD(void, ReleaseFstab, (Fstab *fstab));

    MOCK_METHOD(FstabItem*, FindFstabItemForMountPoint, (Fstab fstab, const char *mp));

    MOCK_METHOD(int, ParseFstabPerLine, (char *str, Fstab *fstab, bool procMounts, const char *separator));

    MOCK_METHOD(int, GetBlockDeviceByMountPoint, (const char *mountPoint, const Fstab *fstab, char *deviceName, int nameLen));

    MOCK_METHOD(int, GetBlockDeviceByName, (const char *deviceName, const Fstab *fstab, char* miscDev, size_t size));

    MOCK_METHOD(bool, IsSupportedFilesystem, (const char *fsType));

    MOCK_METHOD(int, DoFormat, (const char *devPath, const char *fsType));

    MOCK_METHOD(int, MountOneItem, (FstabItem *item));

    MOCK_METHOD(MountStatus, GetMountStatusForMountPoint, (const char *mp));

    MOCK_METHOD(int, MountAllWithFstabFile, (const char *fstabFile, bool required));

    MOCK_METHOD(int, MountAllWithFstab, (const Fstab *fstab, bool required));

    MOCK_METHOD(int, UmountAllWithFstabFile, (const char *file));

    MOCK_METHOD(int, MountOneWithFstabFile, (const char *fstabFile, const char *devName, bool required));

    MOCK_METHOD(int, FsManagerDmRemoveDevice, (const char *devName));

    MOCK_METHOD(int, GetBlockDevicePath, (const char *partName, char *path, size_t size));

    MOCK_METHOD(int, UpdateUserDataMEDevice, (FstabItem *item));

    MOCK_METHOD(uint64_t, LookupErofsEnd, (const char *dev));

    MOCK_METHOD(void, SetRemountFlag, (bool flag));

    MOCK_METHOD(bool, GetRemountFlag, ());

    MOCK_METHOD(int, LoadFscryptPolicy, (char *buf, size_t size));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_FS_MANAGER_MOCK_H
