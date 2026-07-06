/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "remount_innerkits_unittest_common.h"

using namespace testing::ext;

static int g_stubResult[STUB_MAX] = {};
int g_mountStubCalls = 0;
int g_umount2StubCalls = 0;
char g_mountStubTarget[MAX_BUFFER_LEN] = {};
char g_mountStubData[MAX_BUFFER_LEN] = {};

extern "C" {
void SetStubResult(STUB_TYPE type, int result)
{
    if (type >= STUB_SPRINTF && type < STUB_MAX) {
        g_stubResult[type] = result;
    }
}

int GetStubResult(STUB_TYPE type)
{
    if (type < STUB_SPRINTF || type >= STUB_MAX) {
        return -1;
    }
    return g_stubResult[type];
}

int SprintfStub(char *buffer, size_t size, const char *fmt, ...)
{
    if (buffer == nullptr || size == 0 || fmt == nullptr) {
        return -1;
    }
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf_s(buffer, size, size - 1, fmt, args);
    va_end(args);
    return (ret < 0 || static_cast<size_t>(ret) >= size) ? -1 : ret;
}

int MountStub(const char *source, const char *target,
    const char *filesystemtype, unsigned long mountflags, const void *data)
{
    (void)source;
    (void)filesystemtype;
    (void)mountflags;
    g_mountStubCalls++;
    if (target != nullptr) {
        (void)snprintf_s(g_mountStubTarget, sizeof(g_mountStubTarget), sizeof(g_mountStubTarget) - 1,
            "%s", target);
    }
    if (data != nullptr) {
        (void)snprintf_s(g_mountStubData, sizeof(g_mountStubData), sizeof(g_mountStubData) - 1,
            "%s", static_cast<const char *>(data));
    }
    if (g_stubResult[STUB_MOUNT] != 0) {
        errno = EINVAL;
    }
    return g_stubResult[STUB_MOUNT];
}

int UmountStub(const char *target)
{
    (void)target;
    return g_stubResult[STUB_UMOUNT];
}

int Umount2Stub(const char *target, int flags)
{
    (void)target;
    (void)flags;
    g_umount2StubCalls++;
    return g_stubResult[STUB_UMOUNT];
}

int ExecvStub(const char *pathname, char *const argv[])
{
    (void)pathname;
    (void)argv;
    return 0;
}

void RetriggerDmUeventByPathStub(int sockFd, char *path, char **devices, int num)
{
    (void)sockFd;
    (void)path;
    (void)devices;
    (void)num;
}

int UeventdSocketInitStub(void)
{
    return -1;
}
}

TestState g_state;
int g_fakeMountFile;
const SUPPORTED_FILE_SYSTEM g_erofsFileSystem = {"erofs", 0};
const SUPPORTED_FILE_SYSTEM *g_extendedFileSystems[] = {&g_erofsFileSystem, nullptr};

FstabItem *MakeFstabItem(const FstabEntry &entry)
{
    FstabItem *item = static_cast<FstabItem *>(calloc(1, sizeof(FstabItem)));
    if (item == nullptr) {
        return nullptr;
    }
    item->deviceName = strdup(entry.device.c_str());
    item->mountPoint = strdup(entry.mountPoint.c_str());
    item->fsType = strdup(entry.fsType.empty() ? "erofs" : entry.fsType.c_str());
    item->mountOptions = strdup("ro");
    if (item->deviceName == nullptr || item->mountPoint == nullptr ||
        item->fsType == nullptr || item->mountOptions == nullptr) {
        ReleaseFstabItem(item);
        return nullptr;
    }
    return item;
}

Fstab *BuildFstab(const std::vector<FstabEntry> &entries)
{
    Fstab *fstab = static_cast<Fstab *>(calloc(1, sizeof(Fstab)));
    if (fstab == nullptr) {
        return nullptr;
    }
    for (const auto &entry : entries) {
        FstabItem *item = MakeFstabItem(entry);
        if (item == nullptr) {
            ReleaseFstab(fstab);
            return nullptr;
        }
        if (fstab->tail == nullptr) {
            fstab->head = item;
            fstab->tail = item;
        } else {
            fstab->tail->next = item;
            fstab->tail = item;
        }
    }
    return fstab;
}

DmMergeCollectCtx MakeCollectCtx(DmLinearTarget *targets, char mntPaths[][MAX_BUFFER_LEN])
{
    DmMergeCollectCtx ctx = {};
    ctx.targets = targets;
    ctx.mntPaths = mntPaths;
    return ctx;
}

int AccessMock(const char *pathname, int mode)
{
    (void)pathname;
    (void)mode;
    g_state.accessCalls++;
    if (g_state.accessRet != 0) {
        errno = ENOENT;
    }
    return g_state.accessRet;
}

int MkdirMock(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    g_state.mkdirCalls++;
    if (g_state.mkdirFailAt > 0 && g_state.mkdirCalls == g_state.mkdirFailAt) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

int OpenMock(const char *pathname, int flag)
{
    (void)pathname;
    (void)flag;
    g_state.openCalls++;
    int ret = g_state.openRet;
    if (static_cast<size_t>(g_state.openCalls - 1) < g_state.openSeq.size()) {
        ret = g_state.openSeq[g_state.openCalls - 1];
    }
    if (ret < 0) {
        errno = ENOENT;
    }
    return ret;
}

int CloseMock(int fd)
{
    (void)fd;
    g_state.closeCalls++;
    return 0;
}

ssize_t PreadMock(int fd, void *const buf, size_t count, off_t offset)
{
    (void)fd;
    (void)offset;
    g_state.preadCalls++;
    if (!g_state.mockPread) {
        return RealPread(fd, buf, count, offset);
    }
    if (g_state.preadRet <= 0) {
        return g_state.preadRet;
    }
    size_t copyLen = std::min(static_cast<size_t>(g_state.preadRet), count);
    copyLen = std::min(copyLen, g_state.preadContent.size());
    if (copyLen > 0) {
        (void)memcpy_s(buf, count, g_state.preadContent.data(), copyLen);
    }
    if (copyLen < count) {
        static_cast<char *>(buf)[copyLen] = '\0';
    }
    return static_cast<ssize_t>(copyLen);
}

int StatMock(const char *pathname, struct stat *buf)
{
    (void)pathname;
    g_state.statCalls++;
    if (!g_state.mockStat) {
        return RealStat(pathname, buf);
    }
    if (g_state.statRet != 0) {
        errno = ENOENT;
        return g_state.statRet;
    }
    if (buf != nullptr) {
        (void)memset_s(buf, sizeof(*buf), 0, sizeof(*buf));
        buf->st_mode = S_IFREG | TEST_FILE_MODE;
    }
    return 0;
}

int IoctlMock(int fd, int req, va_list args)
{
    (void)fd;
    g_state.ioctlCalls++;
    if (static_cast<unsigned long>(req) == BLKGETSIZE64) {
        uint64_t *size = va_arg(args, uint64_t *);
        if (size != nullptr) {
            *size = g_state.ioctlDeviceSize;
        }
        return 0;
    }
    if (static_cast<unsigned long>(req) == BLKDISCARD) {
        (void)va_arg(args, uint64_t *);
        return 0;
    }
    return 0;
}

int MountRecordMock(const char *source, const char *target, const char *fsType, unsigned long flags, const void *data)
{
    (void)flags;
    g_mountStubCalls++;
    g_state.systemMountCalls++;
    g_state.lastMountSource = source == nullptr ? "" : source;
    g_state.lastMountTarget = target == nullptr ? "" : target;
    g_state.lastMountFsType = fsType == nullptr ? "" : fsType;
    g_state.lastMountData = data == nullptr ? "" : static_cast<const char *>(data);
    if (target != nullptr) {
        (void)snprintf_s(g_mountStubTarget, sizeof(g_mountStubTarget), sizeof(g_mountStubTarget) - 1,
            "%s", target);
    }
    if (data != nullptr) {
        (void)snprintf_s(g_mountStubData, sizeof(g_mountStubData), sizeof(g_mountStubData) - 1,
            "%s", static_cast<const char *>(data));
    }
    if (g_stubResult[STUB_MOUNT] != 0) {
        errno = EINVAL;
    }
    return g_stubResult[STUB_MOUNT];
}

void *CallocMock(size_t m, size_t n)
{
    g_state.callocCalls++;
    if (g_state.callocFailAt > 0 && g_state.callocCalls == g_state.callocFailAt) {
        return nullptr;
    }
    return RealCalloc(m, n);
}

char *StrdupMock(const char *string)
{
    g_state.strdupCalls++;
    if (g_state.strdupFailAt > 0 && g_state.strdupCalls == g_state.strdupFailAt) {
        return nullptr;
    }
    return RealStrdup(string);
}

void ExpectUniqueMountedOverlayPaths()
{
}
extern "C" {
Fstab *WrapLoadFstabFromCommandLine(void)
{
    if (!g_state.mockLoadFstab) {
        return RealLoadFstabFromCommandLine();
    }
    if (g_state.loadFstabNull) {
        return nullptr;
    }
    return BuildFstab(g_state.fstabEntries);
}

Fstab *WrapReadFstabFromFile(const char *file, bool procMounts)
{
    if (!g_state.mockReadFstab) {
        return RealReadFstabFromFile(file, procMounts);
    }
    (void)file;
    (void)procMounts;
    g_state.readFstabCalls++;
    return BuildFstab(g_state.fstabEntries);
}

int WrapGetParameterFromCmdLine(const char *paramName, char *value, size_t valueLen)
{
    if (!g_state.mockGetParameterFromCmdLine) {
        return RealGetParameterFromCmdLine(paramName, value, valueLen);
    }
    if (g_state.getParameterRet == 0 && value != nullptr && valueLen > 0) {
        int ret = snprintf_s(value, valueLen, valueLen - 1, "%s", g_state.hardware.c_str());
        if (ret < 0 || static_cast<size_t>(ret) >= valueLen) {
            return -1;
        }
    }
    return g_state.getParameterRet;
}

int WrapInUpdaterMode(void)
{
    if (g_state.mockInUpdaterMode) {
        return g_state.updaterMode;
    }
    return RealInUpdaterMode();
}

int WrapGetBootSlots(void)
{
    if (g_state.mockGetBootSlots) {
        return g_state.bootSlots;
    }
    return RealGetBootSlots();
}

int WrapGetCurrentSlot(void)
{
    if (g_state.mockGetCurrentSlot) {
        return g_state.currentSlot;
    }
    return RealGetCurrentSlot();
}

int WrapGetMapperAddrForMerge(const char *dev, uint64_t *start, uint64_t *length)
{
    if (!g_state.mockMapper) {
        return RealGetMapperAddrForMerge(dev, start, length);
    }
    g_state.mapperCalls++;
    g_state.lastMapperDevice = dev == nullptr ? "" : dev;
    if (g_state.mapperRet == 0) {
        *start = g_state.mapperStart;
        *length = g_state.mapperLength;
    }
    return g_state.mapperRet;
}

int WrapFsDmCreateLinearDevice(const char *devName, char *dmBlkName,
    uint64_t dmBlkNameLen, DmVerityTarget *target)
{
    if (!g_state.mockCreateLinear) {
        return RealFsDmCreateLinearDevice(devName, dmBlkName, dmBlkNameLen, target);
    }
    (void)target;
    g_state.createLinearCalls++;
    g_state.lastLinearDeviceName = devName == nullptr ? "" : devName;
    if (g_state.createLinearRet == 0 && dmBlkName != nullptr && dmBlkNameLen > 0) {
        int ret = snprintf_s(dmBlkName, dmBlkNameLen, dmBlkNameLen - 1, "/dev/block/dm-linear");
        if (ret < 0 || static_cast<uint64_t>(ret) >= dmBlkNameLen) {
            return -1;
        }
    }
    return g_state.createLinearRet;
}

int WrapFsDmCreateMultiTargetLinearDevice(const char *devName, char *dmDevPath, uint64_t dmDevPathLen,
    DmLinearTarget *target, uint32_t targetNum)
{
    if (!g_state.mockCreateDm) {
        return RealFsDmCreateMultiTargetLinearDevice(devName, dmDevPath, dmDevPathLen, target, targetNum);
    }
    (void)devName;
    (void)target;
    g_state.createDmCalls++;
    g_state.lastTargetNum = targetNum;
    if (g_state.createDmRet == 0 && dmDevPath != nullptr && dmDevPathLen > 0) {
        int ret = snprintf_s(dmDevPath, dmDevPathLen, dmDevPathLen - 1, "/dev/block/dm-test");
        if (ret < 0 || static_cast<uint64_t>(ret) >= dmDevPathLen) {
            return -1;
        }
    }
    return g_state.createDmRet;
}

int WrapFsDmInitDmDev(char *devPath, bool useSocket)
{
    if (!g_state.mockInitDm) {
        return RealFsDmInitDmDev(devPath, useSocket);
    }
    (void)devPath;
    (void)useSocket;
    g_state.initDmCalls++;
    return g_state.initDmRet;
}

int WrapFsDmRemoveDevice(const char *devName)
{
    if (!g_state.mockRemoveDm) {
        return RealFsDmRemoveDevice(devName);
    }
    g_state.removeDmCalls++;
    g_state.removedDmNames.emplace_back(devName == nullptr ? "" : devName);
    return 0;
}

int WrapDmGetDeviceName(int fd, const char *devName, char *outDevName, const uint64_t outDevNameLen)
{
    if (!g_state.mockDmGetDeviceName) {
        return RealDmGetDeviceName(fd, devName, outDevName, outDevNameLen);
    }
    (void)fd;
    (void)devName;
    int ret = g_state.dmGetDeviceNameRet;
    if (static_cast<size_t>(g_state.dmGetDeviceNameCalls) < g_state.dmGetDeviceNameSeq.size()) {
        ret = g_state.dmGetDeviceNameSeq[g_state.dmGetDeviceNameCalls];
    }
    g_state.dmGetDeviceNameCalls++;
    if (ret == 0 && outDevName != nullptr && outDevNameLen > 0) {
        int copyRet = snprintf_s(outDevName, outDevNameLen, outDevNameLen - 1,
            "%s", g_state.dmGetDevicePath.c_str());
        if (copyRet < 0 || static_cast<uint64_t>(copyRet) >= outDevNameLen) {
            return -1;
        }
    }
    return ret;
}

void WrapWaitForFile(const char *source, unsigned int maxSecond)
{
    if (!g_state.mockWaitForFile) {
        RealWaitForFile(source, maxSecond);
        return;
    }
    (void)source;
    (void)maxSecond;
    g_state.waitForFileCalls++;
}

int WrapFormatExt4(const char *fsBlkDev, const char *fsMntPoint)
{
    if (!g_state.mockFormatExt4) {
        return RealFormatExt4(fsBlkDev, fsMntPoint);
    }
    (void)fsBlkDev;
    (void)fsMntPoint;
    g_state.formatExt4Calls++;
    return g_state.formatExt4Ret;
}

bool WrapCheckIsExt4(const char *dev, uint64_t offset)
{
    if (!g_state.mockCheckExt4) {
        return RealCheckIsExt4(dev, offset);
    }
    (void)dev;
    (void)offset;
    g_state.checkExt4Calls++;
    return g_state.checkExt4Ret;
}

bool WrapCheckIsErofs(const char *dev)
{
    if (!g_state.mockCheckErofs) {
        return RealCheckIsErofs(dev);
    }
    (void)dev;
    g_state.checkErofsCalls++;
    return g_state.checkErofsRet;
}

bool WrapIsOverlayEnable(void)
{
    if (!g_state.mockOverlayEnable) {
        return RealIsOverlayEnable();
    }
    g_state.overlayEnableCalls++;
    return g_state.overlayEnableRet;
}

int WrapMountOverlayOne(const char *mnt, const char *overlayPrefix)
{
    if (!g_state.mockMountOverlay) {
        return RealMountOverlayOne(mnt, overlayPrefix);
    }
    (void)overlayPrefix;
    g_state.mountOverlayCalls++;
    g_state.mountedOverlayPaths.emplace_back(mnt == nullptr ? "" : mnt);
    return g_state.mountOverlayRet;
}

int WrapDoMountOverlayDevice(FstabItem *item)
{
    if (!g_state.mockDoMountOverlayDevice) {
        return RealDoMountOverlayDevice(item);
    }
    (void)item;
    g_state.doMountOverlayDeviceCalls++;
    return g_state.doMountOverlayDeviceRet;
}

void WrapOverlayRemountVendorPre(void)
{
    if (!g_state.mockMountOverlay) {
        RealOverlayRemountVendorPre();
        return;
    }
    g_state.vendorPreCalls++;
}

void WrapOverlayRemountVendorPost(void)
{
    if (!g_state.mockMountOverlay) {
        RealOverlayRemountVendorPost();
        return;
    }
    g_state.vendorPostCalls++;
}

int WrapSwitchRoot(const char *newRoot)
{
    if (!g_state.mockSwitchRoot) {
        return RealSwitchRoot(newRoot);
    }
    (void)newRoot;
    g_state.switchRootCalls++;
    return g_state.switchRootRet;
}

void WrapSetRemountResultFlag(void)
{
    if (!g_state.mockSetRemountResultFlag) {
        RealSetRemountResultFlag();
        return;
    }
    g_state.setRemountResultFlagCalls++;
}

int WrapGetRemountResult(void)
{
    if (g_state.mockGetRemountResult) {
        return g_state.remountResult;
    }
    return RealGetRemountResult();
}

bool WrapIsDmMergeOverlayActive(void)
{
    if (g_state.mockDmMergeActive) {
        return g_state.dmMergeActive;
    }
    return RealIsDmMergeOverlayActive();
}

bool WrapIsDmMergeRemountEnabled(void)
{
    if (g_state.mockDmMergeEnabled) {
        return g_state.dmMergeEnabled;
    }
    return RealIsDmMergeRemountEnabled();
}

int WrapTryDmMergeRemount(void)
{
    if (!g_state.mockTryDmMergeRemount) {
        return RealTryDmMergeRemount();
    }
    g_state.tryDmMergeRemountCalls++;
    return g_state.tryDmMergeRemountRet;
}

void WrapEngFilesOverlay(const char *source, const char *target)
{
    if (!g_state.mockEngFilesOverlay) {
        RealEngFilesOverlay(source, target);
        return;
    }
    (void)source;
    (void)target;
    g_state.engFilesOverlayCalls++;
}

FILE *WrapSetMntEnt(const char *filename, const char *type)
{
    if (!g_state.mockMountTable) {
        return RealSetMntEnt(filename, type);
    }
    (void)filename;
    (void)type;
    g_state.setmntentCalls++;
    if (g_state.setmntentFails) {
        return nullptr;
    }
    return reinterpret_cast<FILE *>(&g_fakeMountFile);
}

struct mntent *WrapGetMntEnt(FILE *stream)
{
    if (!g_state.mockMountTable) {
        return RealGetMntEnt(stream);
    }
    (void)stream;
    g_state.getmntentCalls++;
    return nullptr;
}

int WrapEndMntEnt(FILE *stream)
{
    if (!g_state.mockMountTable) {
        return RealEndMntEnt(stream);
    }
    (void)stream;
    g_state.endmntentCalls++;
    return 1;
}

ssize_t WrapPwrite(int fd, const void *buf, size_t count, off_t offset)
{
    g_state.pwriteCalls++;
    if (!g_state.mockPwrite) {
        return RealPwrite(fd, buf, count, offset);
    }
    (void)fd;
    (void)buf;
    (void)offset;
    return static_cast<ssize_t>(count);
}

int WrapUnlink(const char *pathname)
{
    g_state.unlinkCalls++;
    if (!g_state.mockUnlink) {
        return RealUnlink(pathname);
    }
    (void)pathname;
    if (g_state.unlinkRet != 0) {
        errno = ENOENT;
    }
    return g_state.unlinkRet;
}

int WrapRmdir(const char *pathname)
{
    g_state.rmdirCalls++;
    if (!g_state.mockRmdir) {
        return RealRmdir(pathname);
    }
    (void)pathname;
    return 0;
}

DIR *WrapOpenDir(const char *name)
{
    g_state.opendirCalls++;
    if (!g_state.mockDirOps) {
        return RealOpenDir(name);
    }
    (void)name;
    if (!g_state.dirOpsOpenSucceeds) {
        errno = ENOENT;
        return nullptr;
    }
    return reinterpret_cast<DIR *>(0x1);
}

struct dirent *WrapReadDir(DIR *dirp)
{
    g_state.readdirCalls++;
    if (!g_state.mockDirOps) {
        return RealReadDir(dirp);
    }
    (void)dirp;
    if (g_state.dirEntryIndex >= g_state.dirEntries.size()) {
        return nullptr;
    }
    static struct dirent entry;
    (void)memset_s(&entry, sizeof(entry), 0, sizeof(entry));
    const auto &fakeEntry = g_state.dirEntries[g_state.dirEntryIndex++];
    (void)snprintf_s(entry.d_name, sizeof(entry.d_name), sizeof(entry.d_name) - 1,
        "%s", fakeEntry.first.c_str());
    entry.d_type = fakeEntry.second;
    return &entry;
}

int WrapCloseDir(DIR *dirp)
{
    g_state.closedirCalls++;
    if (!g_state.mockDirOps) {
        return RealCloseDir(dirp);
    }
    (void)dirp;
    return 0;
}

int WrapLstat(const char *pathname, struct stat *buf)
{
    g_state.lstatCalls++;
    if (!g_state.mockLstat) {
        return RealLstat(pathname, buf);
    }
    (void)pathname;
    if (g_state.lstatRet != 0) {
        errno = ENOENT;
        return g_state.lstatRet;
    }
    if (buf != nullptr) {
        (void)memset_s(buf, sizeof(*buf), 0, sizeof(*buf));
        buf->st_mode = S_IFDIR | TEST_DIR_MODE;
    }
    return 0;
}

int WrapUmount2(const char *target, int flags)
{
    (void)target;
    (void)flags;
    g_state.umount2Calls++;
    return 0;
}

void WrapSync(void)
{
    g_state.syncCalls++;
}

int WrapSystemReadParam(const char *name, char *value, uint32_t *len)
{
    if (!g_state.mockSystemReadParam) {
        return RealSystemReadParam(name, value, len);
    }
    (void)name;
    g_state.systemReadParamCalls++;
    if (g_state.systemReadParamRet != 0) {
        return g_state.systemReadParamRet;
    }
    if (value == nullptr || len == nullptr || *len == 0) {
        return -1;
    }
    int ret = snprintf_s(value, *len, *len - 1, "%s", g_state.systemParamValue.c_str());
    if (ret < 0 || static_cast<uint32_t>(ret) >= *len) {
        return -1;
    }
    *len = static_cast<uint32_t>(strlen(value));
    return 0;
}
}

namespace init_ut {
void RemountDmMergeUnitTest::SetUp()
{
    g_state = {};
    g_mountStubCalls = 0;
    g_umount2StubCalls = 0;
    g_mountStubTarget[0] = '\0';
    g_mountStubData[0] = '\0';
    g_state.mockGetBootSlots = true;
    g_state.bootSlots = 1;
    g_bootSlots = 1;
    g_currentSlot = 1;
    InitSetExtendedFileSystems(g_extendedFileSystems);
    SetStubResult(STUB_MOUNT, 0);
    UpdateAccessFunc(AccessMock);
    UpdateMkdirFunc(MkdirMock);
    UpdateCloseFunc(CloseMock);
    UpdateMountFunc(MountStub);
    UpdatePreadFunc(PreadMock);
    UpdateStatFunc(StatMock);
    UpdateIoctlFunc(IoctlMock);
    UpdateCallocFunc(nullptr);
    UpdateStrdupFunc(nullptr);
}

void RemountDmMergeUnitTest::TearDown()
{
    UpdateOpenFunc(nullptr);
    UpdateAccessFunc(nullptr);
    UpdateMkdirFunc(nullptr);
    UpdateCloseFunc(nullptr);
    UpdateMountFunc(nullptr);
    UpdatePreadFunc(nullptr);
    UpdateStatFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    UpdateCallocFunc(nullptr);
    UpdateStrdupFunc(nullptr);
    InitSetExtendedFileSystems(nullptr);
    g_bootSlots = 1;
    g_currentSlot = 1;
    SetStubResult(STUB_MOUNT, 0);
    g_state = {};
}

void RemountDmMergeUnitTest::UseDefaultDmMergeMocks()
{
    g_state.mockLoadFstab = true;
    g_state.mockMapper = true;
    g_state.mockCreateDm = true;
    g_state.mockInitDm = true;
    g_state.mockRemoveDm = true;
    g_state.mockWaitForFile = true;
    g_state.mockFormatExt4 = true;
    g_state.mockMountOverlay = true;
    g_state.mockSetRemountResultFlag = true;
    g_state.mockEngFilesOverlay = true;
}

void RemountDmMergeUnitTest::UseFirstStageDmMergeMocks()
{
    g_state.mockMapper = true;
    g_state.mapperStart = TEST_MAPPER_START;
    g_state.mapperLength = TEST_MAPPER_LENGTH;
    g_state.mockCreateDm = true;
    g_state.mockCreateLinear = true;
    g_state.mockInitDm = true;
    g_state.mockRemoveDm = true;
    g_state.mockWaitForFile = true;
    g_state.mockCheckExt4 = true;
    g_state.checkExt4Ret = true;
    g_state.mockLstat = true;
    g_state.lstatRet = -1;
}

void RemountDmMergeUnitTest::UseActiveDmMergeDeviceMocks()
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = TEST_OPEN_FD;
    g_state.mockDmGetDeviceName = true;
    g_state.dmGetDeviceNameRet = 0;
    g_state.mockInitDm = true;
    g_state.mockRemoveDm = true;
    g_state.mockWaitForFile = true;
}

HWTEST_F(RemountDmMergeUnitTest, RemountRofsOverlay_LastDmMergeActiveSkips, TestSize.Level0)
{
    g_state.mockGetRemountResult = true;
    g_state.remountResult = REMOUNT_SUCC;
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;

    EXPECT_EQ(RemountRofsOverlay(), REMOUNT_SUCC);
}

HWTEST_F(RemountDmMergeUnitTest, RemountRofsOverlay_LastLegacyOverlaySkips, TestSize.Level0)
{
    g_state.mockGetRemountResult = true;
    g_state.remountResult = REMOUNT_SUCC;
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = false;

    EXPECT_EQ(RemountRofsOverlay(), REMOUNT_SUCC);
}

HWTEST_F(RemountDmMergeUnitTest, RemountRofsOverlay_DmMergeDisabledUsesFallback, TestSize.Level0)
{
    g_state.mockGetRemountResult = true;
    g_state.remountResult = REMOUNT_FAIL;
    g_state.mockDmMergeEnabled = true;
    g_state.dmMergeEnabled = false;
    g_state.mockMountTable = true;
    g_state.setmntentFails = true;

    EXPECT_EQ(RemountRofsOverlay(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, RemountRofsOverlay_DmMergeSuccessReturnsSuccess, TestSize.Level0)
{
    g_state.mockGetRemountResult = true;
    g_state.remountResult = REMOUNT_FAIL;
    g_state.mockDmMergeEnabled = true;
    g_state.dmMergeEnabled = true;
    g_state.mockTryDmMergeRemount = true;
    g_state.tryDmMergeRemountRet = REMOUNT_SUCC;

    EXPECT_EQ(RemountRofsOverlay(), REMOUNT_SUCC);
}

HWTEST_F(RemountDmMergeUnitTest, RemountRofsOverlay_DmMergeFailureFallsBack, TestSize.Level0)
{
    g_state.mockGetRemountResult = true;
    g_state.remountResult = REMOUNT_FAIL;
    g_state.mockDmMergeEnabled = true;
    g_state.dmMergeEnabled = true;
    g_state.mockTryDmMergeRemount = true;
    g_state.tryDmMergeRemountRet = REMOUNT_FAIL;
    g_state.mockMountTable = true;
    g_state.mockSetRemountResultFlag = true;

    EXPECT_EQ(RemountRofsOverlay(), REMOUNT_SUCC);
}

HWTEST_F(RemountDmMergeUnitTest, RemountOverlay_DmMergeActiveSkipsLegacyOverlay, TestSize.Level0)
{
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mockMountOverlay = true;

    EXPECT_EQ(RemountOverlay(), 0);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeOverlay_SuccessWithExt4DmMerge, TestSize.Level0)
{
    UseFirstStageDmMergeMocks();
    Fstab *fstab = BuildFstab({{"/dev/block/dm-0", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);

    EXPECT_EQ(TryDmMergeOverlay(fstab), 0);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeOverlay_BootSlotsAdjustsPartitionName, TestSize.Level0)
{
    UseFirstStageDmMergeMocks();
    g_state.bootSlots = 2;
    g_state.mockGetCurrentSlot = true;
    g_state.currentSlot = 2;
    Fstab *fstab = BuildFstab({{"/dev/block/by-name/system", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);

    EXPECT_EQ(TryDmMergeOverlay(fstab), 0);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeOverlay_NonExt4RemovesDmMerge, TestSize.Level0)
{
    UseFirstStageDmMergeMocks();
    g_state.checkExt4Ret = false;
    Fstab *fstab = BuildFstab({{"/dev/block/dm-0", "/", "erofs"}});
    ASSERT_NE(fstab, nullptr);

    EXPECT_EQ(TryDmMergeOverlay(fstab), -1);

    ReleaseFstab(fstab);
}

HWTEST_F(RemountDmMergeUnitTest, MountDmMergeOverlayAll_InactiveSkips, TestSize.Level0)
{
    UpdateOpenFunc(OpenMock);
    g_state.openRet = -1;

    EXPECT_EQ(MountDmMergeOverlayAll(), 0);
}

HWTEST_F(RemountDmMergeUnitTest, MountDmMergeOverlayAll_MarkerMissingRemovesDevice, TestSize.Level0)
{
    UseActiveDmMergeDeviceMocks();
    g_state.accessRet = -1;

    EXPECT_EQ(MountDmMergeOverlayAll(), -1);
}

HWTEST_F(RemountDmMergeUnitTest, MountDmMergeOverlayAll_SuccessMountsUniqueOverlayPaths, TestSize.Level0)
{
    UseActiveDmMergeDeviceMocks();
    g_state.mockMountOverlay = true;

    EXPECT_EQ(MountDmMergeOverlayAll(), 0);
    ExpectUniqueMountedOverlayPaths();
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeRemountEnabled_ParamTrueDoesNotBlockHyperholdEnable, TestSize.Level0)
{
    g_state.mockSystemReadParam = true;
    g_state.systemReadParamRet = 0;
    g_state.systemParamValue = "true";
    UpdateOpenFunc(OpenMock);
    g_state.mockPread = true;
    g_state.preadContent = HYPERHOLD_SWITCH_ENABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_TRUE(IsDmMergeRemountEnabled());
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeRemountEnabled_ParamFalseDoesNotBlockHyperholdEnable, TestSize.Level0)
{
    g_state.mockSystemReadParam = true;
    g_state.systemReadParamRet = 0;
    g_state.systemParamValue = "false";
    UpdateOpenFunc(OpenMock);
    g_state.mockPread = true;
    g_state.preadContent = HYPERHOLD_SWITCH_ENABLE;
    g_state.preadRet = static_cast<ssize_t>(g_state.preadContent.size());

    EXPECT_TRUE(IsDmMergeRemountEnabled());
}

HWTEST_F(RemountDmMergeUnitTest, IsDmMergeRemountEnabled_HyperholdOpenFailureReturnsFalse, TestSize.Level0)
{
    g_state.mockSystemReadParam = true;
    g_state.systemReadParamRet = 0;
    g_state.systemParamValue = "true";
    UpdateOpenFunc(OpenMock);
    g_state.openRet = -1;

    EXPECT_FALSE(IsDmMergeRemountEnabled());
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_InactiveFails, TestSize.Level0)
{
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = false;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), -1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_SuccessMountsRofsAndLower, TestSize.Level0)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), 0);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, DoMountDmMergeErofsOnly_UsrSuccessSwitchesRoot, TestSize.Level0)
{
    UseFirstStageDmMergeMocks();
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mockSwitchRoot = true;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/usr", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(DoMountDmMergeErofsOnly(item), 0);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, MountItemByFsType_OverlayDisabledFallsBackToPlainMount, TestSize.Level0)
{
    g_state.mockCheckErofs = true;
    g_state.checkErofsRet = true;
    g_state.mockOverlayEnable = true;
    g_state.overlayEnableRet = false;
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mockDoMountOverlayDevice = true;
    g_state.mockStat = true;
    g_state.statRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);
    MountResult result = {};

    EXPECT_EQ(MountItemByFsType(item, &result), 0);
    EXPECT_EQ(g_mountStubCalls, 1);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, MountErofsOverlayItem_DmMergeActiveUsesErofsOnlyPath, TestSize.Level0)
{
    UseFirstStageDmMergeMocks();
    g_state.mockOverlayEnable = true;
    g_state.overlayEnableRet = true;
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = true;
    g_state.mockDoMountOverlayDevice = true;
    g_state.doMountOverlayDeviceRet = -1;
    g_state.mockStat = true;
    g_state.statRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(MountErofsOverlayItem(item), 0);
    EXPECT_EQ(g_mountStubCalls, 2);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, MountErofsOverlayItem_DmMergeInactiveUsesLegacyOverlayPath, TestSize.Level0)
{
    g_state.mockOverlayEnable = true;
    g_state.overlayEnableRet = true;
    g_state.mockDmMergeActive = true;
    g_state.dmMergeActive = false;
    g_state.mockDoMountOverlayDevice = true;
    g_state.doMountOverlayDeviceRet = 0;
    g_state.mockStat = true;
    g_state.statRet = -1;
    FstabItem *item = MakeFstabItem({"/dev/block/dm-0", "/vendor", "erofs"});
    ASSERT_NE(item, nullptr);

    EXPECT_EQ(MountErofsOverlayItem(item), 0);

    ReleaseFstabItem(item);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_LoadFstabFails, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.loadFstabNull = true;
    g_state.mockInUpdaterMode = true;
    g_state.updaterMode = 0;
    g_state.mockGetParameterFromCmdLine = true;
    g_state.getParameterRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_ReadFstabFallbackSuccess, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.loadFstabNull = true;
    g_state.mockReadFstab = true;
    g_state.mockInUpdaterMode = true;
    g_state.updaterMode = 0;
    g_state.mockGetParameterFromCmdLine = true;
    g_state.getParameterRet = 0;
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_SUCC);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_UpdaterFstabFileMissingFails, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.loadFstabNull = true;
    g_state.mockInUpdaterMode = true;
    g_state.updaterMode = 1;
    g_state.accessRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_BootSlotsAdjustsPartitionName, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.bootSlots = 2;
    g_state.mockGetCurrentSlot = true;
    g_state.currentSlot = 1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_SUCC);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_CollectEmptyFails, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-data", "/data", "ext4"}};

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_CreateDeviceFailure, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.createDmRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_InitDeviceFailureRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.initDmRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_DeviceNotReadyRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.accessRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_FormatFailureRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.formatExt4Ret = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_MkdirMergeRootFailureRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.mkdirFailAt = 1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_MountExt4FailureRemovesDevice, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    SetStubResult(STUB_MOUNT, -1);

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_MkdirOverlayDirsFailureRollsBack, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {{"/dev/block/dm-0", "/", "erofs"}};
    g_state.mkdirFailAt = 3;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_MountOverlayFailureRollsBack, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {
        {"/dev/block/dm-0", "/", "erofs"},
        {"/dev/block/dm-1", "/vendor", "erofs"},
    };
    g_state.mountOverlayRet = -1;

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_FAIL);
}

HWTEST_F(RemountDmMergeUnitTest, TryDmMergeRemount_SuccessMountsDmMergeOverlay, TestSize.Level0)
{
    UseDefaultDmMergeMocks();
    g_state.fstabEntries = {
        {"/dev/block/dm-0", "/", "erofs"},
        {"/dev/block/dm-1", "/vendor", "erofs"},
    };

    EXPECT_EQ(TryDmMergeRemount(), REMOUNT_SUCC);
}
} // namespace init_ut
