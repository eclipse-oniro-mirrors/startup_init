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
#ifndef FS_DM_MOCK_H
#define FS_DM_MOCK_H
#include "fs_dm.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

// for wrapper FsDmInitDmDev;
int __real_FsDmInitDmDev(char *devPath, bool useSocket);
typedef int (*FsdminitdmdevFunc)(char *devPath, bool useSocket);
void UpdateFsdminitdmdevFunc(FsdminitdmdevFunc func);

// for wrapper FsDmCreateDevice;
int __real_FsDmCreateDevice(char **dmDevPath, const char *devName, DmVerityTarget *target);
typedef int (*FsdmcreatedeviceFunc)(char **dmDevPath, const char *devName, DmVerityTarget *target);
void UpdateFsdmcreatedeviceFunc(FsdmcreatedeviceFunc func);

// for wrapper FsDmRemoveDevice;
int __real_FsDmRemoveDevice(const char *devName);
typedef int (*FsdmremovedeviceFunc)(const char *devName);
void UpdateFsdmremovedeviceFunc(FsdmremovedeviceFunc func);

// for wrapper LoadDmDeviceTable;
int __real_LoadDmDeviceTable(int fd, const char *devName, DmVerityTarget *target, int dmType);
typedef int (*LoaddmdevicetableFunc)(int fd, const char *devName, DmVerityTarget *target, int dmType);
void UpdateLoaddmdevicetableFunc(LoaddmdevicetableFunc func);

// for wrapper DmGetDeviceName;
int __real_DmGetDeviceName(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen);
typedef int (*DmgetdevicenameFunc)(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen);
void UpdateDmgetdevicenameFunc(DmgetdevicenameFunc func);

// for wrapper ActiveDmDevice;
int __real_ActiveDmDevice(int fd, const char *devName);
typedef int (*ActivedmdeviceFunc)(int fd, const char *devName);
void UpdateActivedmdeviceFunc(ActivedmdeviceFunc func);

// for wrapper InitDmIo;
int __real_InitDmIo(struct dm_ioctl *io, const char *devName);
typedef int (*InitdmioFunc)(struct dm_ioctl *io, const char *devName);
void UpdateInitdmioFunc(InitdmioFunc func);

// for wrapper CreateDmDev;
int __real_CreateDmDev(int fd, const char *devName);
typedef int (*CreatedmdevFunc)(int fd, const char *devName);
void UpdateCreatedmdevFunc(CreatedmdevFunc func);

// for wrapper GetDmDevPath;
int __real_GetDmDevPath(int fd, char **dmDevPath, const char *devName);
typedef int (*GetdmdevpathFunc)(int fd, char **dmDevPath, const char *devName);
void UpdateGetdmdevpathFunc(GetdmdevpathFunc func);

// for wrapper GetDmStatusInfo;
bool __real_GetDmStatusInfo(const char *name, struct dm_ioctl *io);
typedef bool (*GetdmstatusinfoFunc)(const char *name, struct dm_ioctl *io);
void UpdateGetdmstatusinfoFunc(GetdmstatusinfoFunc func);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif // TEST_WRAPPER_H