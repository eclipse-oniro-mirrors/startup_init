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
#include "lib_fs_dm_mock.h"

using namespace OHOS::AppSpawn;
int MockFsdminitdmdev(char *devPath, bool useSocket)
{
    return FsDm::fsDmFunc->FsDmInitDmDev(char *devPath, bool useSocket);
}

int MockFsdmcreatedevice(char **dmDevPath, const char *devName, DmVerityTarget *target)
{
    return FsDm::fsDmFunc->FsDmCreateDevice(char **dmDevPath, const char *devName, DmVerityTarget *target);
}

int MockFsdmremovedevice(const char *devName)
{
    return FsDm::fsDmFunc->FsDmRemoveDevice(const char *devName);
}

int MockLoaddmdevicetable(int fd, const char *devName, DmVerityTarget *target, int dmType)
{
    return FsDm::fsDmFunc->LoadDmDeviceTable(int fd, const char *devName, DmVerityTarget *target, int dmType);
}

int MockDmgetdevicename(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen)
{
    return FsDm::fsDmFunc->DmGetDeviceName(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen);
}

int MockActivedmdevice(int fd, const char *devName)
{
    return FsDm::fsDmFunc->ActiveDmDevice(int fd, const char *devName);
}

int MockInitdmio(struct dm_ioctl *io, const char *devName)
{
    return FsDm::fsDmFunc->InitDmIo(struct dm_ioctl *io, const char *devName);
}

int MockCreatedmdev(int fd, const char *devName)
{
    return FsDm::fsDmFunc->CreateDmDev(int fd, const char *devName);
}

int MockGetdmdevpath(int fd, char **dmDevPath, const char *devName)
{
    return FsDm::fsDmFunc->GetDmDevPath(int fd, char **dmDevPath, const char *devName);
}

bool MockGetdmstatusinfo(const char *name, struct dm_ioctl *io)
{
    return FsDm::fsDmFunc->GetDmStatusInfo(const char *name, struct dm_ioctl *io);
}

