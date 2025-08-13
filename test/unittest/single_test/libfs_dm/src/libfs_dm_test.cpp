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
#include "libfs_dm_test.h"
#include <gtest/gtest.h>
#include "func_wrapper.h"
#include "fs_dm_mock.h"
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include "init_utils.h"
#include "securec.h"
using namespace testing;
using namespace testing::ext;
#define STRSIZE 64
#define TESTIODATA "10 10 10"
#define FD_ID 10000
#ifdef __cplusplus
#if __cplusplus
static int g_socketFd = -1;
extern "C"
{
#endif
#endif
void RetriggerDmUeventByPathStub(int sockFd, char *path, char **devices, int num)
{
    printf("RetriggerDmUeventByPathStub");
}

int UeventdSocketInitStub()
{
    return g_socketFd;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
namespace OHOS {
class LibFsDmTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void LibFsDmTest::SetUpTestCase()
{
    GTEST_LOG_(INFO) << "LibFsDmTest SetUpTestCase";
}

void LibFsDmTest::TearDownTestCase()
{
    GTEST_LOG_(INFO) << "LibFsDmTest TearDownTestCase";
}

void LibFsDmTest::SetUp()
{
    GTEST_LOG_(INFO) << "LibFsDmTest SetUp";
}

void LibFsDmTest::TearDown()
{
    GTEST_LOG_(INFO) << "LibFsDmTest TearDown";
}

HWTEST_F(LibFsDmTest, InitDmIo_001, TestSize.Level0)
{
    int ret = InitDmIo(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, InitDmIo_002, TestSize.Level0)
{
    struct dm_ioctl io = {};
    int ret = InitDmIo(&io, nullptr);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(LibFsDmTest, InitDmIo_003, TestSize.Level0)
{
    struct dm_ioctl io = {};
    StrcpySFunc strcpysFunc = [](char *, size_t, const char *) -> int {
        return -1;
    };

    UpdateStrcpySFunc(strcpysFunc);
    int ret = InitDmIo(&io, "devname");
    UpdateStrcpySFunc(nullptr);

    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, InitDmIo_004, TestSize.Level0)
{
    struct dm_ioctl io = {};
    StrcpySFunc strcpysFunc = [](char *, size_t, const char *) -> int {
        return 0;
    };

    UpdateStrcpySFunc(strcpysFunc);
    int ret = InitDmIo(&io, "devname");
    UpdateStrcpySFunc(nullptr);
    EXPECT_EQ(ret, 0);
}


HWTEST_F(LibFsDmTest, CreateDmDev_001, TestSize.Level0)
{
    StrcpySFunc strcpysFunc = [](char *, size_t, const char *) -> int {
        return -1;
    };

    UpdateStrcpySFunc(strcpysFunc);
    int ret = CreateDmDev(0, "devname");
    UpdateStrcpySFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, CreateDmDev_002, TestSize.Level0)
{
    int ret = CreateDmDev(FD_ID, "devname");
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, CreateDmDev_003, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    int ret = CreateDmDev(0, "devname");
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(LibFsDmTest, LoadDmDeviceTable_001, TestSize.Level0)
{
    int dmTpyeIndex = MAXNUMTYPE + 1;
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;
    int ret = LoadDmDeviceTable(FD_ID, nullptr, &target, dmTpyeIndex);
    EXPECT_EQ(ret, -1);
    free(target.paras);
}


HWTEST_F(LibFsDmTest, LoadDmDeviceTable_002, TestSize.Level0)
{
    int dmTpyeIndex = VERIFY;
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;

    CallocFunc callocFunc = [](size_t, size_t) -> void* {
        return nullptr;
    };
    UpdateCallocFunc(callocFunc);
    int ret = LoadDmDeviceTable(FD_ID, nullptr, &target, dmTpyeIndex);
    EXPECT_EQ(ret, -1);
    free(target.paras);
    UpdateCallocFunc(nullptr);
}

HWTEST_F(LibFsDmTest, LoadDmDeviceTable_003, TestSize.Level0)
{
    int dmTpyeIndex = VERIFY;
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;

    StrcpySFunc strcpysFunc = [](char *, size_t, const char *) -> int {
        return -1;
    };

    UpdateStrcpySFunc(strcpysFunc);
    int ret = LoadDmDeviceTable(FD_ID, "devname", &target, dmTpyeIndex);
    EXPECT_EQ(ret, -1);
    UpdateStrcpySFunc(nullptr);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, LoadDmDeviceTable_004, TestSize.Level0)
{
    int dmTpyeIndex = SNAPSHOT;
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;

    int ret = LoadDmDeviceTable(FD_ID, "devname", &target, dmTpyeIndex);
    EXPECT_EQ(ret, -1);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, LoadDmDeviceTable_005, TestSize.Level0)
{
    int dmTpyeIndex = SNAPSHOT;
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;

    StrcpySFunc strcpyFunc = [](char *, size_t, const char *) -> int {
        static int count = 0;
        count ++;
        return count == 2 ? -1 :0;
    };
    UpdateStrcpySFunc(strcpyFunc);
    int ret = LoadDmDeviceTable(FD_ID, "devname", &target, dmTpyeIndex);
    EXPECT_EQ(ret, -1);
    UpdateStrcpySFunc(nullptr);

    free(target.paras);
}

HWTEST_F(LibFsDmTest, LoadDmDeviceTable_006, TestSize.Level0)
{
    int dmTpyeIndex = SNAPSHOT;
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;

    StrcpySFunc strcpyFunc = [](char *, size_t, const char *) -> int {
        static int count = 0;
        count ++;
        return count == 3 ? -1 :0;
    };
    UpdateStrcpySFunc(strcpyFunc);
    int ret = LoadDmDeviceTable(FD_ID, "devname", &target, dmTpyeIndex);
    EXPECT_EQ(ret, -1);
    UpdateStrcpySFunc(nullptr);

    free(target.paras);
}

HWTEST_F(LibFsDmTest, LoadDmDeviceTable_007, TestSize.Level0)
{
    int dmTpyeIndex = VERIFY;
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;


    int ret = LoadDmDeviceTable(FD_ID, "devname", &target, dmTpyeIndex);
    EXPECT_EQ(ret, -1);
    UpdateIoctlFunc(nullptr);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, LoadDmDeviceTable_008, TestSize.Level0)
{
    int dmTpyeIndex = SNAPSHOT;
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;

    IoctlFunc ioctlFunc = [](int, int, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    int ret = LoadDmDeviceTable(FD_ID, "devname", &target, dmTpyeIndex);
    EXPECT_EQ(ret, 0);
    UpdateIoctlFunc(nullptr);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, ActiveDmDevice_001, TestSize.Level0)
{
    StrcpySFunc strcpysFunc = [](char *, size_t, const char *) -> int {
        return -1;
    };

    UpdateStrcpySFunc(strcpysFunc);
    int ret = ActiveDmDevice(FD_ID, "devname");
    EXPECT_EQ(ret, -1);
    UpdateStrcpySFunc(nullptr);
}

HWTEST_F(LibFsDmTest, ActiveDmDevice_002, TestSize.Level0)
{

    IoctlFunc ioctlFunc = [](int, int, va_list) -> int {
        return -1;
    };
    UpdateIoctlFunc(ioctlFunc);
    int ret = ActiveDmDevice(FD_ID, "devname");
    EXPECT_EQ(ret, -1);
    UpdateIoctlFunc(nullptr);
}

HWTEST_F(LibFsDmTest, ActiveDmDevice_004, TestSize.Level0)
{

    IoctlFunc ioctlFunc = [](int, int, va_list) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    int ret = ActiveDmDevice(FD_ID, "devname");
    EXPECT_EQ(ret, 0);
    UpdateIoctlFunc(nullptr);
}

HWTEST_F(LibFsDmTest, GetDmDevPath_001, TestSize.Level0)
{
    StrcpySFunc strcpysFunc = [](char *, size_t, const char *) -> int {
        return -1;
    };
    UpdateStrcpySFunc(strcpysFunc);
    char *dmDevPath = NULL;
    int ret = GetDmDevPath(FD_ID, &dmDevPath, "devname");
    EXPECT_EQ(ret, -1);
    UpdateStrcpySFunc(nullptr);
}

HWTEST_F(LibFsDmTest, GetDmDevPath_002, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int, int, va_list) -> int {
        return -1;
    };
    UpdateIoctlFunc(ioctlFunc);
    char *dmDevPath = NULL;
    int ret = GetDmDevPath(FD_ID, &dmDevPath, "devname");
    EXPECT_EQ(ret, -1);
    UpdateIoctlFunc(nullptr);
}

HWTEST_F(LibFsDmTest, GetDmDevPath_003, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int, int, va_list) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    CallocFunc callocFunc = [](size_t, size_t) -> void* {
        return nullptr;
    };
    UpdateCallocFunc(callocFunc);
    char *dmDevPath = NULL;
    int ret = GetDmDevPath(FD_ID, &dmDevPath, "devname");
    EXPECT_EQ(ret, -1);
    UpdateIoctlFunc(nullptr);
    UpdateCallocFunc(nullptr);
}

HWTEST_F(LibFsDmTest, GetDmDevPath_004, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int, int, va_list) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    SnprintfSFunc snprintfsFunc = [](char *, size_t, size_t, const char *, va_list) -> size_t {
        return -1;
    };
    UpdateSnprintfSFunc(snprintfsFunc);
    char *dmDevPath = NULL;
    int ret = GetDmDevPath(FD_ID, &dmDevPath, "devname");
    EXPECT_EQ(ret, -1);
    UpdateIoctlFunc(nullptr);
    UpdateSnprintfSFunc(nullptr);
}

HWTEST_F(LibFsDmTest, GetDmDevPath_005, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int, int, va_list) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    SnprintfSFunc snprintfsFunc = [](char *, size_t, size_t, const char *, va_list) -> size_t {
        return 0;
    };
    UpdateSnprintfSFunc(snprintfsFunc);
    char *dmDevPath = NULL;
    int ret = GetDmDevPath(FD_ID, &dmDevPath, "devname");
    EXPECT_EQ(ret, 0);
    UpdateIoctlFunc(nullptr);
    UpdateSnprintfSFunc(nullptr);
}

HWTEST_F(LibFsDmTest, FsDmCreateDevice_001, TestSize.Level0)
{
    OpenFunc openFunc = [](const char *, int) -> int {
        return -1;
    };
    UpdateOpenFunc(openFunc);
    char *dmDevPath = nullptr;
    int ret = FsDmCreateDevice(&dmDevPath, "devname", nullptr);
    EXPECT_EQ(ret, -1);
    UpdateOpenFunc(nullptr);
}

HWTEST_F(LibFsDmTest, FsDmCreateDevice_002, TestSize.Level0)
{
    OpenFunc openFunc = [](const char *, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int, int, va_list args) -> int {
        return -1;
    };
    UpdateIoctlFunc(ioctlFunc);
    char *dmDevPath = nullptr;
    int ret = FsDmCreateDevice(&dmDevPath, "devname", nullptr);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, FsDmCreateDevice_003, TestSize.Level0)
{
    OpenFunc openFunc = [](const char *, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int, int, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char *dmDevPath = nullptr;
    int ret = FsDmCreateDevice(&dmDevPath, "devname", nullptr);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, FsDmCreateDevice_004, TestSize.Level0)
{
    OpenFunc openFunc = [](const char *, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);

    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;

    IoctlFunc ioctlFunc = [](int, int, va_list args) -> int {
        static int count = 0;
        count ++;
        return count == 3 ? -1 : 0;
    };
    UpdateIoctlFunc(ioctlFunc);

    char *dmDevPath = nullptr;
    int ret = FsDmCreateDevice(&dmDevPath, "devname", &target);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    free(target.paras);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, FsDmCreateDevice_005, TestSize.Level0)
{
    OpenFunc openFunc = [](const char *, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);

    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;

    IoctlFunc ioctlFunc = [](int, int, va_list args) -> int {
        static int count = 0;
        count ++;
        return count == 4 ? -1 : 0;
    };
    UpdateIoctlFunc(ioctlFunc);

    char *dmDevPath = nullptr;
    int ret = FsDmCreateDevice(&dmDevPath, "devname", &target);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    free(target.paras);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, FsDmCreateDevice_006, TestSize.Level0)
{
    OpenFunc openFunc = [](const char *, int) -> int {
        return FD_ID;
    };
    UpdateOpenFunc(openFunc);

    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;

    IoctlFunc ioctlFunc = [](int, int, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);

    char *dmDevPath = nullptr;
    int ret = FsDmCreateDevice(&dmDevPath, "devname", &target);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    free(target.paras);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(LibFsDmTest, FsDmInitDmDev_001, TestSize.Level0)
{
    int ret = FsDmInitDmDev(nullptr, false);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, FsDmInitDmDev_002, TestSize.Level0)
{
    SnprintfSFunc snprintfsFunc = [](char *, size_t, size_t, const char *, va_list) -> size_t {
        return -1;
    };
    UpdateSnprintfSFunc(snprintfsFunc);
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmInitDmDev(devpath, false);
    EXPECT_EQ(ret, -1);
    UpdateSnprintfSFunc(nullptr);
}

HWTEST_F(LibFsDmTest, FsDmInitDmDev_003, TestSize.Level0)
{
    CallocFunc callocFunc = [](size_t, size_t) -> void* {
        return nullptr;
    };
    UpdateCallocFunc(callocFunc);
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmInitDmDev(devpath, false);
    EXPECT_EQ(ret, -1);
    UpdateCallocFunc(nullptr);
}

HWTEST_F(LibFsDmTest, FsDmInitDmDev_004, TestSize.Level0)
{
    StrdupFunc strdupFunc = [](const char*) -> char* {
        return nullptr;
    };
    UpdateStrdupFunc(strdupFunc);
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmInitDmDev(devpath, false);
    EXPECT_EQ(ret, -1);
    UpdateStrdupFunc(nullptr);
}

HWTEST_F(LibFsDmTest, FsDmInitDmDev_005, TestSize.Level0)
{
    g_socketFd = -1;
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmInitDmDev(devpath, true);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, FsDmInitDmDev_006, TestSize.Level0)
{
    g_socketFd = FD_ID;
    char devpath[PATH_MAX] = "devpath";
    int ret = FsDmInitDmDev(devpath, true);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(LibFsDmTest, FsDmRemoveDevice_001, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return -1;
    };
    UpdateOpenFunc(openFunc);
    int ret = FsDmRemoveDevice("devname");
    UpdateOpenFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, FsDmRemoveDevice_002, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    int ret = FsDmRemoveDevice("devname");
    UpdateOpenFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, FsDmRemoveDevice_003, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    StrcpySFunc strcpysFunc = [](char *, size_t, const char *) -> int {
        return -1;
    };

    UpdateStrcpySFunc(strcpysFunc);
    int ret = FsDmRemoveDevice("devname");
    UpdateOpenFunc(nullptr);
    UpdateStrcpySFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, FsDmRemoveDevice_004, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    int ret = FsDmRemoveDevice("devname");
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(LibFsDmTest, DmGetDeviceName_001, TestSize.Level0)
{
    StrcpySFunc strcpysFunc = [](char *, size_t, const char *) -> int {
        return -1;
    };
    UpdateStrcpySFunc(strcpysFunc);
    char outname[PATH_MAX];
    int ret = DmGetDeviceName(FD_ID, "devname", outname, PATH_MAX);
    UpdateStrcpySFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, DmGetDeviceName_002, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return -1;
    };
    UpdateIoctlFunc(ioctlFunc);
    char outname[PATH_MAX];
    int ret = DmGetDeviceName(FD_ID, "devname", outname, PATH_MAX);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, DmGetDeviceName_003, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    CallocFunc callocFunc = [](size_t, size_t) -> void* {
        return nullptr;
    };
    UpdateCallocFunc(callocFunc);
    char outname[PATH_MAX];
    int ret = DmGetDeviceName(FD_ID, "devname", outname, PATH_MAX);
    UpdateIoctlFunc(nullptr);
    UpdateCallocFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, DmGetDeviceName_004, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    SnprintfSFunc snprintfsFunc = [](char *, size_t, size_t, const char *, va_list) -> size_t {
        return -1;
    };
    UpdateSnprintfSFunc(snprintfsFunc);
    char outname[PATH_MAX];
    int ret = DmGetDeviceName(FD_ID, "devname", outname, PATH_MAX);
    UpdateIoctlFunc(nullptr);
    UpdateSnprintfSFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, DmGetDeviceName_005, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    MemcpySFunc memscpysFunc = [](void *, size_t, const void *, size_t) -> int {
        return -1;
    };
    UpdateMemcpySFunc(memscpysFunc);
    char outname[PATH_MAX];
    int ret = DmGetDeviceName(FD_ID, "devname", outname, PATH_MAX);
    UpdateIoctlFunc(nullptr);
    UpdateMemcpySFunc(nullptr);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(LibFsDmTest, DmGetDeviceName_006, TestSize.Level0)
{
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);

    char outname[PATH_MAX];
    int ret = DmGetDeviceName(FD_ID, "devname", outname, PATH_MAX);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(LibFsDmTest, FsDmCreateLinearDevice_001, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return -1;
    };
    UpdateOpenFunc(openFunc);
    char outname[PATH_MAX];
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;
    int ret = FsDmCreateLinearDevice("devname", outname, PATH_MAX, &target);
    UpdateOpenFunc(nullptr);
    EXPECT_EQ(ret, -1);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, FsDmCreateLinearDevice_002, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return -1;
    };
    UpdateIoctlFunc(ioctlFunc);
    char outname[PATH_MAX];
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;
    int ret = FsDmCreateLinearDevice("devname", outname, PATH_MAX, &target);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, -1);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, FsDmCreateLinearDevice_003, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        static int count = 0;
        count ++;
        return count == 2 ? -1 :0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char outname[PATH_MAX];
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;
    int ret = FsDmCreateLinearDevice("devname", outname, PATH_MAX, &target);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, -1);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, FsDmCreateLinearDevice_004, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        static int count = 0;
        count ++;
        return count == 3 ? -1 :0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char outname[PATH_MAX];
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;
    int ret = FsDmCreateLinearDevice("devname", outname, PATH_MAX, &target);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, -1);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, FsDmCreateLinearDevice_005, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        static int count = 0;
        count ++;
        return count == 4 ? -1 : 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char outname[PATH_MAX];
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;
    int ret = FsDmCreateLinearDevice("devname", outname, PATH_MAX, &target);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, -1);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, FsDmCreateLinearDevice_006, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    char outname[PATH_MAX];
    DmVerityTarget target = {};
    target.paras = (char *)malloc(sizeof(char) * PATH_MAX);
    target.paras_len = sizeof(char) * PATH_MAX;
    int ret = FsDmCreateLinearDevice("devname", outname, PATH_MAX, &target);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, 0);
    free(target.paras);
}

HWTEST_F(LibFsDmTest, GetDmStatusInfo_001, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return -1;
    };
    UpdateOpenFunc(openFunc);
    struct dm_ioctl io;
    int ret = GetDmStatusInfo("devname", &io);
    UpdateOpenFunc(nullptr);
    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmTest, GetDmStatusInfo_002, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    StrcpySFunc strcpysFunc = [](char *, size_t, const char *) -> int {
        return -1;
    };

    UpdateStrcpySFunc(strcpysFunc);
    struct dm_ioctl io;
    int ret = GetDmStatusInfo("devname", &io);
    UpdateOpenFunc(nullptr);
    UpdateStrcpySFunc(nullptr);
    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmTest, GetDmStatusInfo_003, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return -1;
    };
    UpdateIoctlFunc(ioctlFunc);
    struct dm_ioctl io;
    int ret = GetDmStatusInfo("devname", &io);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, false);
}

HWTEST_F(LibFsDmTest, GetDmStatusInfo_004, TestSize.Level0)
{
    OpenFunc openFunc = [](const char*, int) -> int {
        return 0;
    };
    UpdateOpenFunc(openFunc);
    IoctlFunc ioctlFunc = [](int fd, int req, va_list args) -> int {
        return 0;
    };
    UpdateIoctlFunc(ioctlFunc);
    struct dm_ioctl io;
    int ret = GetDmStatusInfo("devname", &io);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    EXPECT_EQ(ret, true);
}
}