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
#ifndef APPSPAWN_FS_DM_MOCK_H
#define APPSPAWN_FS_DM_MOCK_H

int MockFsdminitdmdev(char *devPath, bool useSocket);

int MockFsdmcreatedevice(char **dmDevPath, const char *devName, DmVerityTarget *target);

int MockFsdmremovedevice(const char *devName);

int MockLoaddmdevicetable(int fd, const char *devName, DmVerityTarget *target, int dmType);

int MockDmgetdevicename(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen);

int MockActivedmdevice(int fd, const char *devName);

int MockInitdmio(struct dm_ioctl *io, const char *devName);

int MockCreatedmdev(int fd, const char *devName);

int MockGetdmdevpath(int fd, char **dmDevPath, const char *devName);

bool MockGetdmstatusinfo(const char *name, struct dm_ioctl *io);

namespace OHOS {
namespace AppSpawn {
class FsDm {
public:
    virtual ~FsDm() = default;
    virtual int FsDmInitDmDev(char *devPath, bool useSocket) = 0;

    virtual int FsDmCreateDevice(char **dmDevPath, const char *devName, DmVerityTarget *target) = 0;

    virtual int FsDmRemoveDevice(const char *devName) = 0;

    virtual int LoadDmDeviceTable(int fd, const char *devName, DmVerityTarget *target, int dmType) = 0;

    virtual int DmGetDeviceName(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen) = 0;

    virtual int ActiveDmDevice(int fd, const char *devName) = 0;

    virtual int InitDmIo(struct dm_ioctl *io, const char *devName) = 0;

    virtual int CreateDmDev(int fd, const char *devName) = 0;

    virtual int GetDmDevPath(int fd, char **dmDevPath, const char *devName) = 0;

    virtual bool GetDmStatusInfo(const char *name, struct dm_ioctl *io) = 0;

public:
    static inline std::shared_ptr<FsDm> fsDmFunc = nullptr;
};

class FsDmMock : public FsDm {
public:

    MOCK_METHOD(int, FsDmInitDmDev, (char *devPath, bool useSocket));

    MOCK_METHOD(int, FsDmCreateDevice, (char **dmDevPath, const char *devName, DmVerityTarget *target));

    MOCK_METHOD(int, FsDmRemoveDevice, (const char *devName));

    MOCK_METHOD(int, LoadDmDeviceTable, (int fd, const char *devName, DmVerityTarget *target, int dmType));

    MOCK_METHOD(int, DmGetDeviceName, (int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen));

    MOCK_METHOD(int, ActiveDmDevice, (int fd, const char *devName));

    MOCK_METHOD(int, InitDmIo, (struct dm_ioctl *io, const char *devName));

    MOCK_METHOD(int, CreateDmDev, (int fd, const char *devName));

    MOCK_METHOD(int, GetDmDevPath, (int fd, char **dmDevPath, const char *devName));

    MOCK_METHOD(bool, GetDmStatusInfo, (const char *name, struct dm_ioctl *io));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_FS_DM_MOCK_H
