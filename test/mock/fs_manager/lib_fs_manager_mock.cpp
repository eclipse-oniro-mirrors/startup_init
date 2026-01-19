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
#include "lib_fs_manager_mock.h"

using namespace OHOS::AppSpawn;
Fstab* MockLoadfstabfromcommandline()
{
    return FsManager::fsManagerFunc->LoadFstabFromCommandLine();
}

int MockGetbootslots()
{
    return FsManager::fsManagerFunc->GetBootSlots();
}

int MockGetcurrentslot()
{
    return FsManager::fsManagerFunc->GetCurrentSlot();
}

void MockReleasefstab(Fstab *fstab)
{
    FsManager::fsManagerFunc->ReleaseFstab(Fstab *fstab);
}

FstabItem* MockFindfstabitemformountpoint(Fstab fstab, const char *mp)
{
    return FsManager::fsManagerFunc->FindFstabItemForMountPoint(Fstab fstab, const char *mp);
}

int MockParsefstabperline(char *str, Fstab *fstab, bool procMounts, const char *separator)
{
    return FsManager::fsManagerFunc->ParseFstabPerLine(char *str, Fstab *fstab, bool procMounts, const char *separator);
}

int MockGetblockdevicebymountpoint(const char *mountPoint, const Fstab *fstab, char *deviceName, int nameLen)
{
    return FsManager::fsManagerFunc->GetBlockDeviceByMountPoint(const char *mountPoint, const Fstab *fstab, char *deviceName, int nameLen);
}

int MockGetblockdevicebyname(const char *deviceName, const Fstab *fstab, char* miscDev, size_t size)
{
    return FsManager::fsManagerFunc->GetBlockDeviceByName(const char *deviceName, const Fstab *fstab, char* miscDev, size_t size);
}

bool MockIssupportedfilesystem(const char *fsType)
{
    return FsManager::fsManagerFunc->IsSupportedFilesystem(const char *fsType);
}

int MockDoformat(const char *devPath, const char *fsType)
{
    return FsManager::fsManagerFunc->DoFormat(const char *devPath, const char *fsType);
}

int MockMountoneitem(FstabItem *item)
{
    return FsManager::fsManagerFunc->MountOneItem(FstabItem *item);
}

MountStatus MockGetmountstatusformountpoint(const char *mp)
{
    return FsManager::fsManagerFunc->GetMountStatusForMountPoint(const char *mp);
}

int MockMountallwithfstabfile(const char *fstabFile, bool required)
{
    return FsManager::fsManagerFunc->MountAllWithFstabFile(const char *fstabFile, bool required);
}

int MockMountallwithfstab(const Fstab *fstab, bool required)
{
    return FsManager::fsManagerFunc->MountAllWithFstab(const Fstab *fstab, bool required);
}

int MockUmountallwithfstabfile(const char *file)
{
    return FsManager::fsManagerFunc->UmountAllWithFstabFile(const char *file);
}

int MockMountonewithfstabfile(const char *fstabFile, const char *devName, bool required)
{
    return FsManager::fsManagerFunc->MountOneWithFstabFile(const char *fstabFile, const char *devName, bool required);
}

int MockFsmanagerdmremovedevice(const char *devName)
{
    return FsManager::fsManagerFunc->FsManagerDmRemoveDevice(const char *devName);
}

int MockGetblockdevicepath(const char *partName, char *path, size_t size)
{
    return FsManager::fsManagerFunc->GetBlockDevicePath(const char *partName, char *path, size_t size);
}

int MockUpdateuserdatamedevice(FstabItem *item)
{
    return FsManager::fsManagerFunc->UpdateUserDataMEDevice(FstabItem *item);
}

uint64_t MockLookuperofsend(const char *dev)
{
    return FsManager::fsManagerFunc->LookupErofsEnd(const char *dev);
}

void MockSetremountflag(bool flag)
{
    FsManager::fsManagerFunc->SetRemountFlag(bool flag);
}

bool MockGetremountflag()
{
    return FsManager::fsManagerFunc->GetRemountFlag();
}

int MockLoadfscryptpolicy(char *buf, size_t size)
{
    return FsManager::fsManagerFunc->LoadFscryptPolicy(char *buf, size_t size);
}

