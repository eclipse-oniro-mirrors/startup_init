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
#ifndef APPSPAWN_FS_MANAGER_DEFINE_H
#define APPSPAWN_FS_MANAGER_DEFINE_H

#define LoadFstabFromCommandLine MockLoadfstabfromcommandline

#define GetBootSlots MockGetbootslots

#define GetCurrentSlot MockGetcurrentslot

#define ReleaseFstab MockReleasefstab

#define FindFstabItemForMountPoint MockFindfstabitemformountpoint

#define ParseFstabPerLine MockParsefstabperline

#define GetBlockDeviceByMountPoint MockGetblockdevicebymountpoint

#define GetBlockDeviceByName MockGetblockdevicebyname

#define IsSupportedFilesystem MockIssupportedfilesystem

#define DoFormat MockDoformat

#define MountOneItem MockMountoneitem

#define GetMountStatusForMountPoint MockGetmountstatusformountpoint

#define MountAllWithFstabFile MockMountallwithfstabfile

#define MountAllWithFstab MockMountallwithfstab

#define UmountAllWithFstabFile MockUmountallwithfstabfile

#define MountOneWithFstabFile MockMountonewithfstabfile

#define FsManagerDmRemoveDevice MockFsmanagerdmremovedevice

#define GetBlockDevicePath MockGetblockdevicepath

#define UpdateUserDataMEDevice MockUpdateuserdatamedevice

#define LookupErofsEnd MockLookuperofsend

#define SetRemountFlag MockSetremountflag

#define GetRemountFlag MockGetremountflag

#define LoadFscryptPolicy MockLoadfscryptpolicy

#endif // APPSPAWN_FS_MANAGER_DEFINE_H
