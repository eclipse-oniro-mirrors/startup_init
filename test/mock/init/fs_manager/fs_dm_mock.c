/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "fs_dm_mock.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

// start wrap FsDmInitDmDev
static FsdminitdmdevFunc g_FsDmInitDmDev = NULL;
void UpdateFsdminitdmdevFunc(FsdminitdmdevFunc func)
{
    g_FsDmInitDmDev = func;
}

int __wrap_FsDmInitDmDev(char *devPath, bool useSocket)
{
    if (g_FsDmInitDmDev) {
        return g_FsDmInitDmDev(devPath, useSocket);
    } else {
        return __real_FsDmInitDmDev(devPath, useSocket);
    }
}

// start wrap FsDmCreateDevice
static FsdmcreatedeviceFunc g_FsDmCreateDevice = NULL;
void UpdateFsdmcreatedeviceFunc(FsdmcreatedeviceFunc func)
{
    g_FsDmCreateDevice = func;
}

int __wrap_FsDmCreateDevice(char **dmDevPath, const char *devName, DmVerityTarget *target)
{
    if (g_FsDmCreateDevice) {
        return g_FsDmCreateDevice(dmDevPath, devName, target);
    } else {
        return __real_FsDmCreateDevice(dmDevPath, devName, target);
    }
}

// start wrap FsDmRemoveDevice
static FsdmremovedeviceFunc g_FsDmRemoveDevice = NULL;
void UpdateFsdmremovedeviceFunc(FsdmremovedeviceFunc func)
{
    g_FsDmRemoveDevice = func;
}

int __wrap_FsDmRemoveDevice(const char *devName)
{
    if (g_FsDmRemoveDevice) {
        return g_FsDmRemoveDevice(devName);
    } else {
        return __real_FsDmRemoveDevice(devName);
    }
}

// start wrap LoadDmDeviceTable
static LoaddmdevicetableFunc g_LoadDmDeviceTable = NULL;
void UpdateLoaddmdevicetableFunc(LoaddmdevicetableFunc func)
{
    g_LoadDmDeviceTable = func;
}

int __wrap_LoadDmDeviceTable(int fd, const char *devName, DmVerityTarget *target, int dmType)
{
    if (g_LoadDmDeviceTable) {
        return g_LoadDmDeviceTable(fd, devName, target, dmType);
    } else {
        return __real_LoadDmDeviceTable(fd, devName, target, dmType);
    }
}

// start wrap DmGetDeviceName
static DmgetdevicenameFunc g_DmGetDeviceName = NULL;
void UpdateDmgetdevicenameFunc(DmgetdevicenameFunc func)
{
    g_DmGetDeviceName = func;
}

int __wrap_DmGetDeviceName(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen)
{
    if (g_DmGetDeviceName) {
        return g_DmGetDeviceName(fd, devName, outDevName, outDevNameLen);
    } else {
        return __real_DmGetDeviceName(fd, devName, outDevName, outDevNameLen);
    }
}

// start wrap ActiveDmDevice
static ActivedmdeviceFunc g_ActiveDmDevice = NULL;
void UpdateActivedmdeviceFunc(ActivedmdeviceFunc func)
{
    g_ActiveDmDevice = func;
}

int __wrap_ActiveDmDevice(int fd, const char *devName)
{
    if (g_ActiveDmDevice) {
        return g_ActiveDmDevice(fd, devName);
    } else {
        return __real_ActiveDmDevice(fd, devName);
    }
}

// start wrap InitDmIo
static InitdmioFunc g_InitDmIo = NULL;
void UpdateInitdmioFunc(InitdmioFunc func)
{
    g_InitDmIo = func;
}

int __wrap_InitDmIo(struct dm_ioctl *io, const char *devName)
{
    if (g_InitDmIo) {
        return g_InitDmIo(io, devName);
    } else {
        return __real_InitDmIo(io, devName);
    }
}

// start wrap CreateDmDev
static CreatedmdevFunc g_CreateDmDev = NULL;
void UpdateCreatedmdevFunc(CreatedmdevFunc func)
{
    g_CreateDmDev = func;
}

int __wrap_CreateDmDev(int fd, const char *devName)
{
    if (g_CreateDmDev) {
        return g_CreateDmDev(fd, devName);
    } else {
        return __real_CreateDmDev(fd, devName);
    }
}

// start wrap GetDmDevPath
static GetdmdevpathFunc g_GetDmDevPath = NULL;
void UpdateGetdmdevpathFunc(GetdmdevpathFunc func)
{
    g_GetDmDevPath = func;
}

int __wrap_GetDmDevPath(int fd, char **dmDevPath, const char *devName)
{
    if (g_GetDmDevPath) {
        return g_GetDmDevPath(fd, dmDevPath, devName);
    } else {
        return __real_GetDmDevPath(fd, dmDevPath, devName);
    }
}

// start wrap GetDmStatusInfo
static GetdmstatusinfoFunc g_GetDmStatusInfo = NULL;
void UpdateGetdmstatusinfoFunc(GetdmstatusinfoFunc func)
{
    g_GetDmStatusInfo = func;
}

bool __wrap_GetDmStatusInfo(const char *name, struct dm_ioctl *io)
{
    if (g_GetDmStatusInfo) {
        return g_GetDmStatusInfo(name, io);
    } else {
        return __real_GetDmStatusInfo(name, io);
    }
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
