/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "fstab_mount_test.h"
#include "fs_manager/fs_manager.h"
#include <gtest/gtest.h>
#include "func_wrapper.h"
#include "fsck_group_wrapper.h"
#include <sys/stat.h>
#include <sys/mount.h>
#include "init_utils.h"
#include "securec.h"
#include "erofs_mount_overlay.h"
using namespace testing;
using namespace testing::ext;
#define STRSIZE 64
#define MAX_BUFFER_LEN 256

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

size_t SnprintfSReturnZero(char *strDest, size_t destMax, size_t count, const char *format, va_list args)
{
    return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif


namespace OHOS {
class FstabMountTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void FstabMountTest::SetUpTestCase()
{
    GTEST_LOG_(INFO) << "FstabMountTest SetUpTestCase";
}

void FstabMountTest::TearDownTestCase()
{
    GTEST_LOG_(INFO) << "FstabMountTest TearDownTestCase";
}

void FstabMountTest::SetUp()
{
    GTEST_LOG_(INFO) << "FstabMountTest SetUp";
}

void FstabMountTest::TearDown()
{
    GTEST_LOG_(INFO) << "FstabMountTest TearDown";
}

HWTEST_F(FstabMountTest, DoOneMountItem_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = DoMountOneItem(&item, NULL);
    EXPECT_NE(rc, 0);
}

HWTEST_F(FstabMountTest, DoOneMountItem_002, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "hmfs";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5,checkpoint=disable";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = DoMountOneItem(&item, NULL);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_001, TestSize.Level0)
{
    int rc =  GetDataWithoutCheckpoint(nullptr, 0, nullptr, 0);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_002, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3";
    int rc =  GetDataWithoutCheckpoint(fsData, STRSIZE, nullptr, 0);
    EXPECT_EQ(rc, -1);

    char result[STRSIZE] = {0};
    rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    EXPECT_EQ(rc, 0);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_003, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3,op4,op5";
    char result[STRSIZE] = {0};
    StrdupFunc func = [](const char *path) -> char *
    {
        return nullptr;
    };
    UpdateStrdupFunc(func);
    int rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    UpdateStrdupFunc(nullptr);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_004, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3,op4,op5";
    char result[STRSIZE] = {0};
    MallocFunc func = [](size_t size) -> void* {
        return nullptr;
    };
    UpdateMallocFunc(func);
    int rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    UpdateMallocFunc(nullptr);
    EXPECT_EQ(rc, -1);
}


HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_005, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3,op4,op5";
    char result[STRSIZE] = {0};
    StrncatSFunc func = [](char * strDest, size_t destMax, const char * strSrc, size_t count) -> int {
        if (strcmp(strSrc, ",") == 0) {
            return -1;
        }
        return __real_strncat_s(strDest, destMax, strSrc, count);
    };
    UpdateStrncatSFunc(func);
    int rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    UpdateStrncatSFunc(nullptr);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, GetDataWithoutCheckpoint_006, TestSize.Level0)
{
    char fsData[STRSIZE] = "checkpoint=disable,op1,op2,op3,op4,op5";
    char result[STRSIZE] = {0};
    StrncatSFunc func = [](char *, size_t, const char *, size_t) -> int {
        return -1;
    };
    UpdateStrncatSFunc(func);
    int rc = GetDataWithoutCheckpoint(fsData, STRSIZE, result, STRSIZE);
    UpdateStrncatSFunc(nullptr);
    EXPECT_EQ(rc, -1);
}

HWTEST_F(FstabMountTest, MountWithCheckpoint_001, TestSize.Level0)
{
    StatFunc func = [](const char* path, struct stat *) -> int {
        return 0;
    };
    UpdateStatFunc(func);
    MountResult result = MountWithCheckpoint("/source", "/target", "fsType", 0, "op1,op2,op3,op4,op5");
    UpdateStatFunc(nullptr);
    EXPECT_EQ(result.rc, -1);
}

HWTEST_F(FstabMountTest, MountWithCheckpoint_002, TestSize.Level0)
{
    StatFunc func = [](const char* path, struct stat *) -> int {
        return 0;
    };
    UpdateStatFunc(func);

    MkdirFunc mkdirFunc = [](const char*, mode_t) -> int {
        return 0;
    };
    UpdateMkdirFunc(mkdirFunc);
    UpdateSnprintfSFunc(SnprintfSReturnZero);

    MountResult result = MountWithCheckpoint("/source", "/target", "fsType", 0, "op1,op2,op3,op4,op5");
    UpdateStatFunc(nullptr);
    UpdateMkdirFunc(nullptr);
    UpdateSnprintfSFunc(nullptr);

    GTEST_LOG_(INFO) << "checkpointMountCounter=" << result.checkpointMountCounter;
    EXPECT_EQ(result.rc, -1);
}

HWTEST_F(FstabMountTest, MountWithCheckpoint_003, TestSize.Level0)
{
    StatFunc func = [](const char* path, struct stat *) -> int {
        return 0;
    };
    UpdateStatFunc(func);

    MkdirFunc mkdirFunc = [](const char*, mode_t) -> int {
        return 0;
    };
    UpdateMkdirFunc(mkdirFunc);

    MountFunc mountFunc = [](const char *, const char *, const char *, unsigned long, const void *) -> int {
        errno = EBUSY;
        return -1;
    };
    UpdateMountFunc(mountFunc);

    MountResult result = MountWithCheckpoint("/source", "/target", "fsType", 0, "op1,op2,op3,op4,op5");
    UpdateStatFunc(nullptr);
    UpdateMkdirFunc(nullptr);
    UpdateMountFunc(nullptr);

    GTEST_LOG_(INFO) << "checkpointMountCounter=" << result.checkpointMountCounter;
    EXPECT_EQ(result.rc, 0);
}

// mock state not exist
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 1;
    };
    UpdateAccessFunc(func);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    EXPECT_NE(rc, -1);
}

// mock state exist open state fail
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_002, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 0;
    };
    UpdateAccessFunc(func);

    OpenFunc func2 = [](const char *pathname, int flag) -> int {
        return -1;
    };
    UpdateOpenFunc(func2);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    UpdateAccessFunc(NULL);
    UpdateOpenFunc(NULL);
    EXPECT_EQ(rc, -1);
}

//mock read state suc but state < 0 , record stage
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_003, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 0;
    };
    UpdateAccessFunc(func);

    OpenFunc func2 = [](const char *pathname, int flag) -> int {
        return 1;
    };
    UpdateOpenFunc(func2);

    ReadFunc mock_read_specific_state = [](int fd, void *buf, size_t count) -> ssize_t {
        static int call_count = 0;
        call_count++;
        if (call_count == 1) {
            const char* data = "-100";
            memcpy_s(buf, count, data, strlen(data));
            return strlen(data);
        } else if (call_count == 2) {
            const char* data2 = "100";
            memcpy_s(buf, count, data2, strlen(data2));
            return strlen(data2);
        }
        return -1;
    };
    UpdateReadFunc(mock_read_specific_state);


    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    UpdateAccessFunc(NULL);
    UpdateOpenFunc(NULL);
    UpdateReadFunc(NULL);
    EXPECT_EQ(rc, -1);
}

//mock read state suc read dm fail
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_004, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 0;
    };
    UpdateAccessFunc(func);

    OpenFunc func2 = [](const char *pathname, int flag) -> int {
        return 1;
    };
    UpdateOpenFunc(func2);

    ReadFunc mock_read_specific_state = [](int fd, void *buf, size_t count) -> ssize_t {
        static int call_count = 0;
        call_count++;
        if (call_count == 1) {
            memcpy_s(buf, count, "22873", 5);
            return 5;
        } else if (call_count == 2) {
            errno = EIO;
            return -1;
        }
        return -1;
    };
    UpdateReadFunc(mock_read_specific_state);


    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    UpdateAccessFunc(NULL);
    UpdateOpenFunc(NULL);
    UpdateReadFunc(NULL);
    EXPECT_EQ(rc, -1);
}

//mock read state suc read dm suc GetDmDevPath fail
HWTEST_F(FstabMountTest, UpdateUserDataMEDevice_005, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    AccessFunc func = [](const char *pathname, int mode) -> int {
        return 0;
    };
    UpdateAccessFunc(func);

    OpenFunc func2 = [](const char *pathname, int flag) -> int {
        return 1;
    };
    UpdateOpenFunc(func2);

    ReadFunc mock_read_specific_state = [](int fd, void *buf, size_t count) -> ssize_t {
        static int call_count = 0;
        call_count++;
        if (call_count == 1) {
            memcpy_s(buf, count, "22873", 5);
            return 5;
        } else if (call_count == 2) {
            memcpy_s(buf, count, "22873", 5);
            return 5;
        }
        return -1;
    };
    UpdateReadFunc(mock_read_specific_state);


    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = UpdateUserDataMEDevice(&item);
    UpdateAccessFunc(NULL);
    UpdateOpenFunc(NULL);
    UpdateReadFunc(NULL);
#ifdef SUPPORT_HVB
    EXPECT_EQ(rc, -1);
#else
    EXPECT_NE(rc, -1);
#endif
}

HWTEST_F(FstabMountTest, DoMountOverlayDevice_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = DoMountOverlayDevice(&item);
    EXPECT_NE(rc, 0);
}

HWTEST_F(FstabMountTest, GetOverlayDevice_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    char devRofs[MAX_BUFFER_LEN] = {0};
    char devExt4[MAX_BUFFER_LEN] = {0};
    int rc = 0;
    rc = GetOverlayDevice(&item, devRofs, MAX_BUFFER_LEN, devExt4, MAX_BUFFER_LEN);
    EXPECT_NE(rc, 0);
}

HWTEST_F(FstabMountTest, MountPartitionDevice_001, TestSize.Level0)
{
    char deviceName[STRSIZE]  = "deviceName";
    char mountPoint[STRSIZE] = "mountPoint";
    char fsType[STRSIZE] = "fstype";
    char mountOptions[STRSIZE] = "op1,op2,op3,op4,op5";
    unsigned int fsManagerFlags = 0;

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    char devRofs[MAX_BUFFER_LEN] = {0};
    char devExt4[MAX_BUFFER_LEN] = {0};
    int rc = 0;
    rc = MountPartitionDevice(&item, devRofs, devExt4);
    EXPECT_EQ(rc, 0);
}

// Test DoResizeF2fs with NO_NEED_RESIZE mode (kdump already done)
HWTEST_F(FstabMountTest, MountOneItem_Resize_NO_NEED_RESIZE_001, TestSize.Level0)
{
    char deviceName[STRSIZE] = "/dev/block/by-name/userdata";
    char mountPoint[STRSIZE] = "/data";
    char fsType[STRSIZE] = "f2fs";
    char mountOptions[STRSIZE] = "rw";
    unsigned int fsManagerFlags = 0;

    // Mock NeedDoAllResize to return NO_NEED_RESIZE
    NeedDoAllResizeFunc needDoAllResizeFunc = [](const unsigned int flags) -> int {
        return NO_NEED_RESIZE;
    };
    UpdateNeedDoAllResizeFunc(needDoAllResizeFunc);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    // When NeedDoAllResize returns NO_NEED_RESIZE, DoResizeF2fs should return -1
    // and skip resize operation
    int rc = MountOneItem(&item);
    UpdateNeedDoAllResizeFunc(nullptr);
    EXPECT_EQ(rc, 0);
}

// Test DoResizeF2fs with RESIZE_NORMAL mode
HWTEST_F(FstabMountTest, MountOneItem_Resize_NORMAL_001, TestSize.Level0)
{
    char deviceName[STRSIZE] = "/dev/block/by-name/userdata";
    char mountPoint[STRSIZE] = "/data";
    char fsType[STRSIZE] = "f2fs";
    char mountOptions[STRSIZE] = "rw";
    unsigned int fsManagerFlags = 0;

    AccessFunc accessFunc = [](const char *pathname, int mode) -> int {
        if (strstr(pathname, "resize.f2fs") != nullptr) {
            return 0; // File exists
        }
        return -1;
    };
    UpdateAccessFunc(accessFunc);

    NeedDoAllResizeFunc needDoAllResizeFunc = [](const unsigned int flags) -> int {
        return RESIZE_NORMAL;
    };
    UpdateNeedDoAllResizeFunc(needDoAllResizeFunc);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = MountOneItem(&item);
    UpdateAccessFunc(nullptr);
    UpdateNeedDoAllResizeFunc(nullptr);
    EXPECT_EQ(rc, 0);
}

// Test DoResizeF2fs with RESIZE_META_NO_CHANGE mode
HWTEST_F(FstabMountTest, MountOneItem_Resize_META_NO_CHANGE_001, TestSize.Level0)
{
    char deviceName[STRSIZE] = "/dev/block/by-name/userdata";
    char mountPoint[STRSIZE] = "/data";
    char fsType[STRSIZE] = "f2fs";
    char mountOptions[STRSIZE] = "rw";
    unsigned int fsManagerFlags = 0;

    AccessFunc accessFunc = [](const char *pathname, int mode) -> int {
        if (strstr(pathname, "resize.f2fs") != nullptr) {
            return 0; // File exists
        }
        return -1;
    };
    UpdateAccessFunc(accessFunc);

    NeedDoAllResizeFunc needDoAllResizeFunc = [](const unsigned int flags) -> int {
        return RESIZE_META_NO_CHANGE;
    };
    UpdateNeedDoAllResizeFunc(needDoAllResizeFunc);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = MountOneItem(&item);
    UpdateAccessFunc(nullptr);
    UpdateNeedDoAllResizeFunc(nullptr);
    EXPECT_EQ(rc, 0);
}

// Test DoResizeF2fs with FS_MANAGER_PROJQUOTA flag
HWTEST_F(FstabMountTest, MountOneItem_Resize_WITH_PROJQUOTA_001, TestSize.Level0)
{
    char deviceName[STRSIZE] = "/dev/block/by-name/userdata";
    char mountPoint[STRSIZE] = "/data";
    char fsType[STRSIZE] = "f2fs";
    char mountOptions[STRSIZE] = "rw";
    unsigned int fsManagerFlags = FS_MANAGER_PROJQUOTA;

    AccessFunc accessFunc = [](const char *pathname, int mode) -> int {
        if (strstr(pathname, "resize.f2fs") != nullptr) {
            return 0; // File exists
        }
        return -1;
    };
    UpdateAccessFunc(accessFunc);

    NeedDoAllResizeFunc needDoAllResizeFunc = [](const unsigned int flags) -> int {
        return RESIZE_META_NO_CHANGE;
    };
    UpdateNeedDoAllResizeFunc(needDoAllResizeFunc);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = MountOneItem(&item);
    UpdateAccessFunc(nullptr);
    UpdateNeedDoAllResizeFunc(nullptr);
    EXPECT_EQ(rc, 0);
}

// Test DoResizeF2fs with multiple flags (PROJQUOTA + COMPRESSION)
HWTEST_F(FstabMountTest, MountOneItem_Resize_WITH_MULTIPLE_FLAGS_001, TestSize.Level0)
{
    char deviceName[STRSIZE] = "/dev/block/by-name/userdata";
    char mountPoint[STRSIZE] = "/data";
    char fsType[STRSIZE] = "f2fs";
    char mountOptions[STRSIZE] = "rw";
    unsigned int fsManagerFlags = FS_MANAGER_PROJQUOTA | FS_MANAGER_COMPRESSION;

    AccessFunc accessFunc = [](const char *pathname, int mode) -> int {
        if (strstr(pathname, "resize.f2fs") != nullptr) {
            return 0; // File exists
        }
        return -1;
    };
    UpdateAccessFunc(accessFunc);

    NeedDoAllResizeFunc needDoAllResizeFunc = [](const unsigned int flags) -> int {
        return RESIZE_META_NO_CHANGE;
    };
    UpdateNeedDoAllResizeFunc(needDoAllResizeFunc);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = MountOneItem(&item);
    UpdateAccessFunc(nullptr);
    UpdateNeedDoAllResizeFunc(nullptr);
    EXPECT_EQ(rc, 0);
}

// Test DoResizeF2fs when resize.f2fs file does not exist
HWTEST_F(FstabMountTest, MountOneItem_Resize_FILE_NOT_EXIST_001, TestSize.Level0)
{
    char deviceName[STRSIZE] = "/dev/block/by-name/userdata";
    char mountPoint[STRSIZE] = "/data";
    char fsType[STRSIZE] = "f2fs";
    char mountOptions[STRSIZE] = "rw";
    unsigned int fsManagerFlags = 0;

    AccessFunc accessFunc = [](const char *pathname, int mode) -> int {
        if (strstr(pathname, "resize.f2fs") != nullptr) {
            return -1; // File does not exist
        }
        return -1;
    };
    UpdateAccessFunc(accessFunc);

    NeedDoAllResizeFunc needDoAllResizeFunc = [](const unsigned int flags) -> int {
        return RESIZE_NORMAL;
    };
    UpdateNeedDoAllResizeFunc(needDoAllResizeFunc);

    FstabItem item = {
        .deviceName = deviceName,
        .mountPoint = mountPoint,
        .fsType = fsType,
        .mountOptions = mountOptions,
        .fsManagerFlags = fsManagerFlags,
        .next = NULL,
    };

    int rc = MountOneItem(&item);
    UpdateAccessFunc(nullptr);
    UpdateNeedDoAllResizeFunc(nullptr);
    EXPECT_EQ(rc, 0);
}

// ==================== fsck repair animation tests ====================

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pid_t animation;
    int animationInput;
    bool animChecked;
    bool animFailed;
    bool droppingLine;
    char line[1024];
    size_t used;
} FsckDisplay;

void FsckStartAnimation(FsckDisplay *display);
void FsckHandleLine(FsckDisplay *display);
void FsckSendProgress(FsckDisplay *display);
void FsckConsumeByte(FsckDisplay *display, char byte);
void FsckStopAnimation(FsckDisplay *display);
int RunFsckWithAnimation(int argc, char **argv);
int FsckAddAnimGroup(const char *groupName);

#ifdef __cplusplus
}
#endif

static void SetupFsckMocks();
static void ClearFsckMocks();
static void QueueRead(const std::string &data);
static FsckDisplay MakeFsckDisplay();

class FsckRepairTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() { SetupFsckMocks(); }
    void TearDown() { ClearFsckMocks(); }
};

static int g_forkCalls, g_accessCalls, g_waitpidCalls, g_killCalls, g_closeCalls;
static int g_readCalls, g_writeCalls, g_pipeCalls, g_pipe2Calls, g_pollCalls, g_fcntlCalls;
static int g_getgrnamCalls, g_getgroupsCalls, g_setgroupsCalls;
static pid_t g_forkReturn = 12345;
static int g_accessReturn = 0;
static pid_t g_waitpidReturn = 12345;
static int g_waitpidStatus = 0;
static int g_pipe2Return = 0;
static int g_fcntlReturn = 0;
static pid_t g_lastKillPid = 0;
static int g_lastKillSig = 0;
static std::string g_lastWriteData;
static bool g_getgrnamNull = false;
static gid_t g_graphicsGid = 1003;
static int g_getgroupsCount = 2;
static bool g_getgroupsCountFail = false;
static bool g_getgroupsListFail = false;
static std::vector<gid_t> g_existingGroups;
static int g_setgroupsReturn = 0;
static size_t g_lastSetgroupsSize = 0;
static std::vector<gid_t> g_lastSetgroupsList;

struct ReadChunk { std::string data; bool isError; };
static std::vector<ReadChunk> g_readQueue;
static size_t g_readIndex = 0;

static void ResetFsckCounters()
{
    g_forkCalls = g_accessCalls = g_waitpidCalls = g_killCalls = g_closeCalls = 0;
    g_readCalls = g_writeCalls = g_pipeCalls = g_pipe2Calls = g_pollCalls = g_fcntlCalls = 0;
    g_getgrnamCalls = g_getgroupsCalls = g_setgroupsCalls = 0;
    g_forkReturn = 12345;
    g_accessReturn = 0;
    g_waitpidReturn = 12345;
    g_waitpidStatus = 0;
    g_pipe2Return = 0;
    g_fcntlReturn = 0;
    g_readQueue.clear();
    g_readIndex = 0;
    g_lastWriteData.clear();
    g_getgrnamNull = false;
    g_graphicsGid = 1003;
    g_getgroupsCount = 2;
    g_getgroupsCountFail = false;
    g_getgroupsListFail = false;
    g_existingGroups.clear();
    g_setgroupsReturn = 0;
    g_lastSetgroupsSize = 0;
    g_lastSetgroupsList.clear();
}

static void SetupFsckMocks()
{
    ResetFsckCounters();
    UpdateAccessFunc([](const char *p, int m) -> int { g_accessCalls++; return g_accessReturn; });
    UpdateForkFunc([]() -> pid_t { g_forkCalls++; return g_forkReturn; });
    UpdateWaitpidFunc([](pid_t p, int *s, int o) -> pid_t {
        g_waitpidCalls++; g_lastKillPid = p;
        if (s) *s = g_waitpidStatus; return g_waitpidReturn; });
    UpdateKillFunc([](pid_t p, int s) -> int { g_killCalls++; g_lastKillPid = p; g_lastKillSig = s; return 0; });
    UpdateCloseFunc([](int fd) -> int { g_closeCalls++; return 0; });
    UpdateWriteFunc([](int fd, const void *buf, size_t count) -> ssize_t {
        g_writeCalls++;
        if (count > 0 && buf) g_lastWriteData.assign((const char *)buf, count);
        return (ssize_t)count; });
    UpdateReadFunc([](int fd, void *buf, size_t count) -> ssize_t {
        g_readCalls++;
        if (g_readIndex >= g_readQueue.size()) return 0;
        const auto &c = g_readQueue[g_readIndex++];
        if (c.isError) return -1;
        ssize_t n = (ssize_t)c.data.size();
        if (n > (ssize_t)count) n = (ssize_t)count;
        if (n > 0 && buf) memcpy(buf, c.data.data(), n);
        return n; });
    UpdatePipeFunc([](int fds[2]) -> int { g_pipeCalls++; fds[0] = 200; fds[1] = 201; return 0; });
    UpdatePipe2Func([](int fds[2], int f) -> int {
        g_pipe2Calls++; if (g_pipe2Return == 0) { fds[0] = 200; fds[1] = 201; } return g_pipe2Return; });
    UpdatePollFunc([](struct pollfd *f, unsigned long n, int t) -> int { g_pollCalls++; return 0; });
    UpdateFcntlFunc([](int fd, int fl, unsigned long a) -> int { g_fcntlCalls++; return g_fcntlReturn; }, -1);
    static struct group g_grpStorage;
    static char g_grpName[] = "graphics";
    UpdateGetgrnamFunc([](const char *name) -> struct group * {
        g_getgrnamCalls++;
        if (g_getgrnamNull) return nullptr;
        g_grpStorage.gr_name = g_grpName;
        g_grpStorage.gr_passwd = nullptr;
        g_grpStorage.gr_gid = g_graphicsGid;
        g_grpStorage.gr_mem = nullptr;
        return &g_grpStorage;
    });
    UpdateGetgroupsFunc([](int size, gid_t list[]) -> int {
        g_getgroupsCalls++;
        if (list == nullptr) {
            if (g_getgroupsCountFail) return -1;
            return g_getgroupsCount;
        }
        if (g_getgroupsListFail) return -1;
        int n = (int)g_existingGroups.size();
        if (n > size) n = size;
        for (int i = 0; i < n; i++) list[i] = g_existingGroups[i];
        return n;
    });
    UpdateSetgroupsFunc([](size_t size, const gid_t *list) -> int {
        g_setgroupsCalls++;
        g_lastSetgroupsSize = size;
        g_lastSetgroupsList.assign(list, list + size);
        return g_setgroupsReturn;
    });
}

static void ClearFsckMocks()
{
    UpdateAccessFunc(nullptr); UpdateForkFunc(nullptr); UpdateWaitpidFunc(nullptr);
    UpdateKillFunc(nullptr); UpdateCloseFunc(nullptr); UpdateWriteFunc(nullptr);
    UpdateReadFunc(nullptr); UpdatePipeFunc(nullptr); UpdatePipe2Func(nullptr);
    UpdatePollFunc(nullptr); UpdateFcntlFunc(nullptr, -1);
    UpdateGetgrnamFunc(nullptr); UpdateGetgroupsFunc(nullptr); UpdateSetgroupsFunc(nullptr);
}

static void QueueRead(const std::string &data) { g_readQueue.push_back({data, false}); }

static FsckDisplay MakeFsckDisplay()
{
    FsckDisplay d = {0};
    d.animation = -1;
    d.animationInput = -1;
    return d;
}

HWTEST_F(FsckRepairTest, StartAnim_AlreadyAttempted_Skip, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    d.animChecked = true;
    FsckStartAnimation(&d);
    EXPECT_EQ(g_forkCalls, 0);
}

HWTEST_F(FsckRepairTest, StartAnim_BinaryNotExist_Skip, TestSize.Level0)
{
    g_accessReturn = -1;
    FsckDisplay d = MakeFsckDisplay();
    FsckStartAnimation(&d);
    EXPECT_GE(g_accessCalls, 1);
    EXPECT_EQ(g_forkCalls, 0);
}

HWTEST_F(FsckRepairTest, StartAnim_PipeFail_NoFork, TestSize.Level0)
{
    g_accessReturn = 0;
    g_fcntlReturn = -1;
    FsckDisplay d = MakeFsckDisplay();
    FsckStartAnimation(&d);
    EXPECT_EQ(g_forkCalls, 0);
}

HWTEST_F(FsckRepairTest, StartAnim_ForkFail_NoState, TestSize.Level0)
{
    g_accessReturn = 0;
    g_forkReturn = -1;
    FsckDisplay d = MakeFsckDisplay();
    FsckStartAnimation(&d);
    EXPECT_GE(g_forkCalls, 1);
    EXPECT_EQ(d.animation, -1);
}

HWTEST_F(FsckRepairTest, StartAnim_Success, TestSize.Level0)
{
    g_accessReturn = 0;
    g_forkReturn = 12345;
    FsckDisplay d = MakeFsckDisplay();
    FsckStartAnimation(&d);
    EXPECT_GE(g_forkCalls, 1);
    EXPECT_EQ(d.animation, 12345);
}

HWTEST_F(FsckRepairTest, HandleLine_NoFullFsck_NoAnim, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    const char *t = "checking filesystem";
    memcpy(d.line, t, strlen(t)); d.used = strlen(t);
    FsckHandleLine(&d);
    EXPECT_EQ(d.animChecked, false);
    EXPECT_EQ((int)d.used, 0);
}

HWTEST_F(FsckRepairTest, HandleLine_FullFsck_StartsAnim, TestSize.Level0)
{
    g_accessReturn = 0; g_forkReturn = 12345;
    FsckDisplay d = MakeFsckDisplay();
    const char *t = "Info: FULL_FSCK started";
    memcpy(d.line, t, strlen(t)); d.used = strlen(t);
    FsckHandleLine(&d);
    EXPECT_EQ(d.animChecked, true);
    EXPECT_GE(g_forkCalls, 1);
}

HWTEST_F(FsckRepairTest, HandleLine_ProgressLine_Forwarded, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    d.animationInput = 201;
    const char *t = "[FSCK] Check node 100 / 1000 (10.00%)";
    memcpy(d.line, t, strlen(t)); d.used = strlen(t);
    FsckHandleLine(&d);
    EXPECT_GE(g_writeCalls, 1);
    EXPECT_EQ(g_lastWriteData, std::string("[FSCK] Check node 100 / 1000 (10.00%)\n"));
}

HWTEST_F(FsckRepairTest, HandleLine_NonProgressLine_NotForwarded, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    d.animationInput = 201;
    const char *t = "[FSCK] Found corruption in block 123";
    memcpy(d.line, t, strlen(t)); d.used = strlen(t);
    FsckHandleLine(&d);
    EXPECT_EQ(g_writeCalls, 0);
}

HWTEST_F(FsckRepairTest, SendProgress_NoInput_Skip, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    const char *t = "[FSCK] Check node 30 / 100 (30.00%)";
    memcpy(d.line, t, strlen(t)); d.used = strlen(t);
    FsckSendProgress(&d);
    EXPECT_EQ(g_writeCalls, 0);
}

HWTEST_F(FsckRepairTest, SendProgress_NonProgressPrefix_Skip, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    d.animationInput = 201;
    const char *t = "[FSCK] Fixing node 123";
    memcpy(d.line, t, strlen(t)); d.used = strlen(t);
    FsckSendProgress(&d);
    EXPECT_EQ(g_writeCalls, 0);
}

HWTEST_F(FsckRepairTest, SendProgress_ProgressPrefix_Written, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    d.animationInput = 201;
    const char *t = "[FSCK] Check node 750 / 1000 (75.00%)";
    memcpy(d.line, t, strlen(t)); d.used = strlen(t);
    FsckSendProgress(&d);
    EXPECT_GE(g_writeCalls, 1);
    EXPECT_EQ(g_lastWriteData, std::string("[FSCK] Check node 750 / 1000 (75.00%)\n"));
}

HWTEST_F(FsckRepairTest, SendProgress_WriteFail_CloseInput, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    d.animationInput = 201;
    const char *t = "[FSCK] Check node 100 / 1000 (10.00%)";
    memcpy(d.line, t, strlen(t)); d.used = strlen(t);
    UpdateWriteFunc([](int fd, const void *buf, size_t count) -> ssize_t {
        g_writeCalls++; errno = EPIPE; return -1; });
    FsckSendProgress(&d);
    EXPECT_GE(g_writeCalls, 1);
    EXPECT_EQ(d.animationInput, -1);
}

HWTEST_F(FsckRepairTest, SendProgress_Eagain_Skip, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    d.animationInput = 201;
    const char *t = "[FSCK] Check node 200 / 1000 (20.00%)";
    memcpy(d.line, t, strlen(t)); d.used = strlen(t);
    UpdateWriteFunc([](int fd, const void *buf, size_t count) -> ssize_t {
        g_writeCalls++; errno = EAGAIN; return -1; });
    FsckSendProgress(&d);
    EXPECT_GE(g_writeCalls, 1);
    EXPECT_NE(d.animationInput, -1);
}

HWTEST_F(FsckRepairTest, ConsumeByte_LineTooLong_DroppingLine, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    d.animationInput = 201;
    char longText[1100];
    memset(longText, 'A', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';
    for (size_t i = 0; i < strlen(longText); ++i) FsckConsumeByte(&d, longText[i]);
    EXPECT_EQ(d.droppingLine, true);
    EXPECT_EQ(g_writeCalls, 0);
    FsckConsumeByte(&d, '\n');
    EXPECT_EQ(d.droppingLine, false);
}

HWTEST_F(FsckRepairTest, StopAnim_NoAnimation_NoOp, TestSize.Level0)
{
    FsckDisplay d = MakeFsckDisplay();
    FsckStopAnimation(&d);
    EXPECT_EQ(g_killCalls, 0);
}

HWTEST_F(FsckRepairTest, StopAnim_AnimationExitsOnEof_NoKill, TestSize.Level0)
{
    g_waitpidReturn = 12345;
    FsckDisplay d = MakeFsckDisplay();
    d.animation = 12345; d.animationInput = 201;
    FsckStopAnimation(&d);
    EXPECT_EQ(g_killCalls, 0);
    EXPECT_GE(g_closeCalls, 1);
    EXPECT_EQ(d.animation, -1);
}

HWTEST_F(FsckRepairTest, StopAnim_AnimationTimeout_SigTerm, TestSize.Level0)
{
    g_waitpidReturn = 0;
    FsckDisplay d = MakeFsckDisplay();
    d.animation = 12345; d.animationInput = 201;
    FsckStopAnimation(&d);
    EXPECT_GE(g_killCalls, 1);
    EXPECT_EQ(g_lastKillSig, SIGTERM);
    EXPECT_EQ(d.animation, -1);
}

HWTEST_F(FsckRepairTest, Run_InvalidArgs_ReturnFail, TestSize.Level0)
{
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    EXPECT_EQ(RunFsckWithAnimation(0, argv), -1);
}

HWTEST_F(FsckRepairTest, Run_NullArgv_ReturnFail, TestSize.Level0)
{
    EXPECT_EQ(RunFsckWithAnimation(1, NULL), -1);
}

HWTEST_F(FsckRepairTest, Run_Pipe2Fail_FallbackExecCommand, TestSize.Level0)
{
    g_pipe2Return = -1;
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    RunFsckWithAnimation(1, argv);
    EXPECT_GE(g_pipe2Calls, 1);
    EXPECT_GE(g_forkCalls, 1);
}

HWTEST_F(FsckRepairTest, Run_ForkFail_ReturnFail, TestSize.Level0)
{
    g_forkReturn = -1;
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    EXPECT_EQ(RunFsckWithAnimation(1, argv), -1);
    EXPECT_GE(g_closeCalls, 2);
}

HWTEST_F(FsckRepairTest, Run_NormalExit_ReturnZero, TestSize.Level0)
{
    g_forkReturn = 12345; g_waitpidReturn = 12345; g_waitpidStatus = 0;
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    EXPECT_EQ(RunFsckWithAnimation(1, argv), 0);
}

HWTEST_F(FsckRepairTest, Run_AbnormalExit_ReturnFail, TestSize.Level0)
{
    g_forkReturn = 12345; g_waitpidReturn = 12345; g_waitpidStatus = 0x7f;
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    EXPECT_EQ(RunFsckWithAnimation(1, argv), -1);
}

HWTEST_F(FsckRepairTest, Run_FsckSignaled_ReturnFail, TestSize.Level0)
{
    g_forkReturn = 12345; g_waitpidReturn = 12345; g_waitpidStatus = 9;
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    EXPECT_EQ(RunFsckWithAnimation(1, argv), -1);
}

HWTEST_F(FsckRepairTest, Run_WithFullFsck_ForksAnimation, TestSize.Level0)
{
    g_forkReturn = 12345; g_waitpidReturn = 12345; g_waitpidStatus = 0; g_accessReturn = 0;
    QueueRead("FULL_FSCK started\n");
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    RunFsckWithAnimation(1, argv);
    EXPECT_GE(g_forkCalls, 2);
}

HWTEST_F(FsckRepairTest, Run_WithProgress_ForwardsToAnimation, TestSize.Level0)
{
    g_forkReturn = 12345; g_waitpidReturn = 12345; g_waitpidStatus = 0; g_accessReturn = 0;
    QueueRead("FULL_FSCK started\n[FSCK] Check node 100 / 1000 (10.00%)\n[FSCK] Check node 500 / 1000 (50.00%)\n");
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    RunFsckWithAnimation(1, argv);
    EXPECT_GE(g_writeCalls, 2);
}

HWTEST_F(FsckRepairTest, Run_WithoutFullFsck_NoAnimation, TestSize.Level0)
{
    g_forkReturn = 12345; g_waitpidReturn = 12345; g_waitpidStatus = 0;
    QueueRead("checking filesystem\n");
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    RunFsckWithAnimation(1, argv);
    EXPECT_EQ(g_forkCalls, 1);
}

HWTEST_F(FsckRepairTest, Run_EmptyOutput_NormalExit, TestSize.Level0)
{
    g_forkReturn = 12345; g_waitpidReturn = 12345; g_waitpidStatus = 0;
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    EXPECT_EQ(RunFsckWithAnimation(1, argv), 0);
    EXPECT_EQ(g_forkCalls, 1);
    EXPECT_EQ(g_killCalls, 0);
}

HWTEST_F(FsckRepairTest, Run_MultiLineFullFsckAndProgress, TestSize.Level0)
{
    g_forkReturn = 12345; g_waitpidReturn = 12345; g_waitpidStatus = 0; g_accessReturn = 0;
    QueueRead("some log\nFULL_FSCK started\n[FSCK] Check node 100 / 1000 (10.00%)\n[FSCK] Check node 500 / 1000 (50.00%)\nend\n");
    char *argv[] = { (char *)"/system/bin/fsck.f2fs", NULL };
    RunFsckWithAnimation(1, argv);
    EXPECT_GE(g_forkCalls, 2);
    EXPECT_GE(g_writeCalls, 2);
}

HWTEST_F(FsckRepairTest, DoFsckF2fs_AnimBinNotExist_FallbackExecCommand, TestSize.Level0)
{
    g_forkReturn = 12345;
    g_waitpidReturn = 12345;
    g_waitpidStatus = 0;
    UpdateAccessFunc([](const char *p, int m) -> int {
        g_accessCalls++;
        if (strstr(p, "fsck_animation") != nullptr) {
            return -1;
        }
        return 0;
    });
    FstabItem item = {};
    char dev[] = "/dev/block/by-name/userdata";
    item.deviceName = dev;
    item.mountPoint = (char *)"/data";
    item.fsType = (char *)"f2fs";
    int ret = DoFsckF2fs(&item);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(g_pipe2Calls, 0);
    UpdateAccessFunc(nullptr);
}

// ==================== FsckAddAnimGroup tests ====================

HWTEST_F(FsckRepairTest, AddAnimGroup_GetgrnamFail_ReturnFail, TestSize.Level0)
{
    g_getgrnamNull = true;
    EXPECT_EQ(FsckAddAnimGroup("graphics"), -1);
    EXPECT_GE(g_getgrnamCalls, 1);
    EXPECT_EQ(g_setgroupsCalls, 0);
}

HWTEST_F(FsckRepairTest, AddAnimGroup_GetgroupsCountFail_ReturnFail, TestSize.Level0)
{
    g_getgroupsCountFail = true;
    EXPECT_EQ(FsckAddAnimGroup("graphics"), -1);
    EXPECT_GE(g_getgroupsCalls, 1);
    EXPECT_EQ(g_setgroupsCalls, 0);
}

HWTEST_F(FsckRepairTest, AddAnimGroup_MallocFail_ReturnFail, TestSize.Level0)
{
    UpdateMallocFunc([](size_t size) -> void * {
        if (size == (size_t)(g_getgroupsCount + 1) * sizeof(gid_t)) {
            return nullptr;
        }
        return __real_malloc(size);
    });
    int ret = FsckAddAnimGroup("graphics");
    UpdateMallocFunc(nullptr);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(g_setgroupsCalls, 0);
}

HWTEST_F(FsckRepairTest, AddAnimGroup_GetgroupsListFail_ReturnFail, TestSize.Level0)
{
    g_getgroupsListFail = true;
    EXPECT_EQ(FsckAddAnimGroup("graphics"), -1);
    EXPECT_GE(g_getgroupsCalls, 2);
    EXPECT_EQ(g_setgroupsCalls, 0);
}

HWTEST_F(FsckRepairTest, AddAnimGroup_GidAlreadyInList_ReturnOk_NoSetgroups, TestSize.Level0)
{
    g_existingGroups = {0, g_graphicsGid};
    EXPECT_EQ(FsckAddAnimGroup("graphics"), 0);
    EXPECT_EQ(g_setgroupsCalls, 0);
}

HWTEST_F(FsckRepairTest, AddAnimGroup_SetgroupsFail_ReturnFail, TestSize.Level0)
{
    g_setgroupsReturn = -1;
    EXPECT_EQ(FsckAddAnimGroup("graphics"), -1);
    EXPECT_GE(g_setgroupsCalls, 1);
}

HWTEST_F(FsckRepairTest, AddAnimGroup_Success_ReturnOk, TestSize.Level0)
{
    g_existingGroups = {0, 1000};
    EXPECT_EQ(FsckAddAnimGroup("graphics"), 0);
    EXPECT_GE(g_setgroupsCalls, 1);
    bool found = false;
    for (gid_t g : g_lastSetgroupsList) {
        if (g == g_graphicsGid) found = true;
    }
    EXPECT_EQ(found, true);
}

HWTEST_F(FsckRepairTest, AddAnimGroup_ZeroGroups_ReturnOk, TestSize.Level0)
{
    g_getgroupsCount = 0;
    EXPECT_EQ(FsckAddAnimGroup("graphics"), 0);
    EXPECT_GE(g_setgroupsCalls, 1);
    EXPECT_EQ(g_lastSetgroupsSize, (size_t)1);
}
}