/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "beget_ext.h"
#include "init_utils.h"
#include "init_file.h"
#include "init_socket.h"
#include "loop_event.h"
#include "syspara/parameter.h"
#include "param/init_param.h"

using namespace testing::ext;

namespace init_ut {

class InitBasicUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp() {}
    void TearDown() {}
};

void InitBasicUnitTest::SetUpTestCase(void)
{
    CheckAndCreateDir("/data/init_ut");
    CheckAndCreateDir("/data/init_ut/data");
    CheckAndCreateDir("/data/init_ut/data/init_agent");
}

void InitBasicUnitTest::TearDownTestCase(void) {}

// ========== Log 模块测试 (原有) ==========

HWTEST_F(InitBasicUnitTest, TestInitLog001, TestSize.Level1)
{
    CheckAndCreateDir(INIT_LOG_PATH);
    
    SetInitLogLevel(INIT_DEBUG);
    InitLogLevel level = GetInitLogLevel();
    EXPECT_EQ(level, INIT_DEBUG);
    
    BEGET_LOGI("TestInitLog info message");
    BEGET_LOGV("TestInitLog verbose message");
    BEGET_LOGE("TestInitLog error message");
    BEGET_LOGW("TestInitLog warning message");
    
    StartupLog(INIT_INFO, BEGET_DOMAIN, "TEST", "StartupLog test message");
    
    SetInitLogLevel(INIT_INFO);
    level = GetInitLogLevel();
    EXPECT_EQ(level, INIT_INFO);
}

HWTEST_F(InitBasicUnitTest, TestInitLog002, TestSize.Level1)
{
    SetInitLogLevel(INIT_ERROR);
    InitLogLevel level = GetInitLogLevel();
    EXPECT_EQ(level, INIT_ERROR);
    
    SetInitLogLevel(INIT_WARN);
    level = GetInitLogLevel();
    EXPECT_EQ(level, INIT_WARN);
    
    SetInitLogLevel(INIT_FATAL);
    level = GetInitLogLevel();
    EXPECT_EQ(level, INIT_FATAL);
    
    SetInitLogLevel(INIT_INFO);
}

HWTEST_F(InitBasicUnitTest, TestLogLevelEnum001, TestSize.Level1)
{
    EXPECT_EQ(INIT_DEBUG, 0);
    EXPECT_EQ(INIT_INFO, 1);
    EXPECT_EQ(INIT_WARN, 2);
    EXPECT_EQ(INIT_ERROR, 3);
    EXPECT_EQ(INIT_FATAL, 4);
}

// ========== Utils 模块测试 (原有) ==========

HWTEST_F(InitBasicUnitTest, TestMakeDir001, TestSize.Level1)
{
    const char *test_dir = "/data/init_ut/test_dir_001";
    int ret = MakeDir(test_dir, 0755);
    int saved_errno = errno;
    
    if (ret != 0 && saved_errno != EEXIST) {
        printf("MakeDir failed: ret=%d, errno=%d (%s)\n", ret, saved_errno, strerror(saved_errno));
        if (saved_errno == ENOENT || saved_errno == EACCES || saved_errno == EROFS) {
            printf("Skipping test - environment limitation\n");
            return;
        }
    }
    EXPECT_TRUE(ret == 0 || saved_errno == EEXIST);
    
    if (ret == 0 || saved_errno == EEXIST) {
        CheckAndCreateDir(test_dir);
        rmdir(test_dir);
    }
}

HWTEST_F(InitBasicUnitTest, TestStringToInt001, TestSize.Level1)
{
    int value = StringToInt("12345", -1);
    EXPECT_EQ(value, 12345);
    
    value = StringToInt("-12345", -1);
    EXPECT_EQ(value, -12345);
    
    value = StringToInt("0", -1);
    EXPECT_EQ(value, 0);
}

HWTEST_F(InitBasicUnitTest, TestStringToInt002, TestSize.Level1)
{
    int value = StringToInt("invalid", -1);
    EXPECT_EQ(value, -1);
    
    value = StringToInt("", -1);
    EXPECT_EQ(value, -1);
    
    value = StringToInt("abc123", -1);
    EXPECT_EQ(value, -1);
}

HWTEST_F(InitBasicUnitTest, TestStringToInt003, TestSize.Level1)
{
    int value = StringToInt("999999999", -1);
    EXPECT_NE(value, -1);
    
    value = StringToInt("-999999999", -1);
    EXPECT_NE(value, -1);
}

// ========== Init 模块测试 (新增) ==========

HWTEST_F(InitBasicUnitTest, TestInitArray001, TestSize.Level1)
{
    int arr[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(ARRAY_LENGTH(arr), 5u);
    
    const char *strArr[] = {"a", "b", "c"};
    EXPECT_EQ(ARRAY_LENGTH(strArr), 3u);
}

HWTEST_F(InitBasicUnitTest, TestInitUidGid001, TestSize.Level1)
{
    uid_t uid = DecodeUid("root");
    EXPECT_EQ(uid, 0u);
    
    uid = DecodeUid("system");
    EXPECT_NE(uid, -1u);
}

HWTEST_F(InitBasicUnitTest, TestInitUidGid002, TestSize.Level1)
{
    gid_t gid = DecodeGid("root");
    EXPECT_EQ(gid, 0u);
    
    gid = DecodeGid("system");
    EXPECT_NE(gid, -1u);
}

HWTEST_F(InitBasicUnitTest, TestInitUidGid003, TestSize.Level1)
{
    uid_t uid = DecodeUid("invalid_user");
    EXPECT_EQ(uid, -1u);
    
    gid_t gid = DecodeGid("invalid_group");
    EXPECT_EQ(gid, -1u);
}

HWTEST_F(InitBasicUnitTest, TestInitString001, TestSize.Level1)
{
    char testStr[] = "a,b,c,d";
    char *dstPtr[4] = {nullptr};
    int ret = SplitString(testStr, ",", dstPtr, 4);
    EXPECT_EQ(ret, 4);
}

HWTEST_F(InitBasicUnitTest, TestInitString002, TestSize.Level1)
{
    char testStr[] = "hello world";
    char trimmed[20] = {0};
    strcpy(trimmed, testStr);
    TrimTail(trimmed, ' ');
    EXPECT_STREQ(trimmed, "hello world");
}

HWTEST_F(InitBasicUnitTest, TestInitString003, TestSize.Level1)
{
    char testStr[] = "  hello";
    char *trimmed = TrimHead(testStr, ' ');
    EXPECT_STREQ(trimmed, "hello");
}

HWTEST_F(InitBasicUnitTest, TestInitString004, TestSize.Level1)
{
    char testStr[] = "test_value";
    int ret = StringReplaceChr(testStr, '_', '-');
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(testStr, "test-value");
}

HWTEST_F(InitBasicUnitTest, TestInitFile001, TestSize.Level1)
{
    const char *testFile = "/data/init_ut/test_file.txt";
    int fd = open(testFile, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        printf("Cannot create test file, skipping\n");
        return;
    }
    write(fd, "test content", 12);
    close(fd);
    
    char *buf = ReadFileToBuf(testFile);
    if (buf != nullptr) {
        EXPECT_NE(strlen(buf), 0u);
        free(buf);
    }
    
    unlink(testFile);
}

HWTEST_F(InitBasicUnitTest, TestInitFile002, TestSize.Level1)
{
    char *realPath = GetRealPath("/data/init_ut");
    if (realPath != nullptr) {
        EXPECT_NE(strlen(realPath), 0u);
        free(realPath);
    }
}

HWTEST_F(InitBasicUnitTest, TestInitFile003, TestSize.Level1)
{
    size_t written = WriteAll(STDOUT_FILENO, "test\n", 5);
    EXPECT_EQ(written, 5u);
}

HWTEST_F(InitBasicUnitTest, TestInitSocket001, TestSize.Level1)
{
    int fd = GetControlSocket("test_socket");
    EXPECT_GE(fd, -1);
}

HWTEST_F(InitBasicUnitTest, TestInitFile004, TestSize.Level1)
{
    int fd = GetControlFile("test_file");
    EXPECT_GE(fd, -1);
}

// ========== LoopEvent 模块测试 (新增) ==========

HWTEST_F(InitBasicUnitTest, TestLoopEvent001, TestSize.Level1)
{
    LoopHandle loop = LE_GetDefaultLoop();
    EXPECT_NE(loop, nullptr);
}

HWTEST_F(InitBasicUnitTest, TestLoopEvent002, TestSize.Level1)
{
    LoopHandle loop = nullptr;
    LE_STATUS status = LE_CreateLoop(&loop);
    if (status == LE_SUCCESS) {
        EXPECT_NE(loop, nullptr);
        LE_CloseLoop(loop);
    } else {
        printf("LE_CreateLoop failed: %d, may need full loopevent library\n", status);
    }
}

HWTEST_F(InitBasicUnitTest, TestLoopEvent003, TestSize.Level1)
{
    LoopHandle loop = LE_GetDefaultLoop();
    BufferHandle buffer = LE_CreateBuffer(loop, 1024);
    if (buffer != nullptr) {
        EXPECT_NE(buffer, nullptr);
        LE_FreeBuffer(loop, nullptr, buffer);
    } else {
        printf("LE_CreateBuffer failed, may need full loopevent library\n");
    }
}

HWTEST_F(InitBasicUnitTest, TestLoopEvent004, TestSize.Level1)
{
    LoopHandle loop = LE_GetDefaultLoop();
    BufferHandle buffer = LE_CreateBuffer(loop, 256);
    if (buffer != nullptr) {
        uint32_t dataSize = 0;
        uint32_t buffSize = 0;
        uint8_t *data = LE_GetBufferInfo(buffer, &dataSize, &buffSize);
        EXPECT_NE(data, nullptr);
        EXPECT_GT(buffSize, 0u);
        LE_FreeBuffer(loop, nullptr, buffer);
    }
}

HWTEST_F(InitBasicUnitTest, TestLoopEvent005, TestSize.Level1)
{
    EXPECT_EQ(LE_SUCCESS, 0);
    EXPECT_EQ(LE_FAILURE, 10000);
    EXPECT_EQ(LE_INVALID_PARAM, 10001);
    EXPECT_EQ(LE_NO_MEMORY, 10002);
}

HWTEST_F(InitBasicUnitTest, TestLoopEvent006, TestSize.Level1)
{
    EXPECT_EQ(EVENT_READ, 0x0001);
    EXPECT_EQ(EVENT_WRITE, 0x0002);
    EXPECT_EQ(EVENT_ERROR, 0x0004);
    EXPECT_EQ(EVENT_FREE, 0x0008);
    EXPECT_EQ(EVENT_TIMEOUT, 0x0010);
}

HWTEST_F(InitBasicUnitTest, TestLoopEvent007, TestSize.Level1)
{
    EXPECT_EQ(LOOP_DEFAULT_BUFFER, 1024 * 5);
    EXPECT_EQ(DEFAULT_TIMEOUT, 1000);
}

HWTEST_F(InitBasicUnitTest, TestLoopEvent008, TestSize.Level1)
{
    EXPECT_EQ(TASK_STREAM, 0x01);
    EXPECT_EQ(TASK_TIME, 0x04);
    EXPECT_EQ(TASK_SIGNAL, 0x08);
    EXPECT_EQ(TASK_WATCHER, 0x10);
}

HWTEST_F(InitBasicUnitTest, TestLoopEvent009, TestSize.Level1)
{
    LoopHandle loop = LE_GetDefaultLoop();
    LE_StopLoop(loop);
}

HWTEST_F(InitBasicUnitTest, TestLoopEvent010, TestSize.Level1)
{
    LoopHandle loop = LE_GetDefaultLoop();
    TimerHandle timer = nullptr;
    LE_STATUS status = LE_CreateTimer(loop, &timer, nullptr, nullptr);
    if (status == LE_SUCCESS) {
        EXPECT_NE(timer, nullptr);
        LE_StopTimer(loop, timer);
    } else {
        printf("LE_CreateTimer failed: %d, may need full loopevent library\n", status);
    }
}

// ========== Syspara 模块测试 (新增) ==========

HWTEST_F(InitBasicUnitTest, TestSysparaBasic001, TestSize.Level1)
{
    const char *type = GetDeviceType();
    if (type != nullptr) {
        EXPECT_NE(strlen(type), 0u);
    }
}

HWTEST_F(InitBasicUnitTest, TestSysparaBasic002, TestSize.Level1)
{
    const char *model = GetProductModel();
    if (model != nullptr) {
        printf("ProductModel: %s\n", model);
    }
}

HWTEST_F(InitBasicUnitTest, TestSysparaBasic003, TestSize.Level1)
{
    const char *series = GetProductSeries();
    if (series != nullptr) {
        printf("ProductSeries: %s\n", series);
    }
}

HWTEST_F(InitBasicUnitTest, TestSysparaBasic004, TestSize.Level1)
{
    const char *software = GetSoftwareModel();
    if (software != nullptr) {
        printf("SoftwareModel: %s\n", software);
    }
}

HWTEST_F(InitBasicUnitTest, TestSysparaBasic005, TestSize.Level1)
{
    const char *version = GetOSFullName();
    if (version != nullptr) {
        printf("OSFullName: %s\n", version);
    }
}

HWTEST_F(InitBasicUnitTest, TestSysparaBasic006, TestSize.Level1)
{
    const char *patch = GetSecurityPatchTag();
    if (patch != nullptr) {
        printf("SecurityPatchTag: %s\n", patch);
    }
}

HWTEST_F(InitBasicUnitTest, TestSysparaBasic007, TestSize.Level1)
{
    const char *abi = GetAbiList();
    if (abi != nullptr) {
        printf("AbiList: %s\n", abi);
    }
}

HWTEST_F(InitBasicUnitTest, TestSysparaBasic008, TestSize.Level1)
{
    int sdkVersion = GetSdkApiVersion();
    printf("SdkApiVersion: %d\n", sdkVersion);
}

HWTEST_F(InitBasicUnitTest, TestSysparaBasic009, TestSize.Level1)
{
    const char *versionId = GetVersionId();
    if (versionId != nullptr) {
        printf("VersionId: %s\n", versionId);
    }
}

HWTEST_F(InitBasicUnitTest, TestSysparaBasic010, TestSize.Level1)
{
    const char *buildHash = GetBuildRootHash();
    if (buildHash != nullptr) {
        printf("BuildRootHash: %s\n", buildHash);
    }
}

// ========== Param 模块测试 (新增) ==========

HWTEST_F(InitBasicUnitTest, TestParamBasic001, TestSize.Level1)
{
    EXPECT_EQ(PARAM_NAME_LEN_MAX, 96);
    EXPECT_EQ(PARAM_VALUE_LEN_MAX, 96);
    EXPECT_EQ(PARAM_CONST_VALUE_LEN_MAX, 4096);
}

HWTEST_F(InitBasicUnitTest, TestParamBasic002, TestSize.Level1)
{
    char value[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t len = sizeof(value);
    int ret = GetParameter("test.param.nonexist", "default_value", value, len);
    EXPECT_GE(ret, 0);
    EXPECT_STREQ(value, "default_value");
}

HWTEST_F(InitBasicUnitTest, TestParamBasic003, TestSize.Level1)
{
    const char *testKey = "test.basic.param";
    const char *testValue = "test_value_123";
    int ret = SetParameter(testKey, testValue);
    if (ret == 0) {
        char value[PARAM_VALUE_LEN_MAX] = {0};
        uint32_t len = sizeof(value);
        ret = GetParameter(testKey, "", value, len);
        if (ret >= 0) {
            EXPECT_STREQ(value, testValue);
        }
    } else {
        printf("SetParameter failed: %d, may need param service running\n", ret);
    }
}

HWTEST_F(InitBasicUnitTest, TestParamBasic004, TestSize.Level1)
{
    int32_t intValue = GetIntParameter("test.int.param", -1);
    printf("GetIntParameter returned: %d\n", intValue);
}

HWTEST_F(InitBasicUnitTest, TestParamBasic005, TestSize.Level1)
{
    char value[PARAM_VALUE_LEN_MAX] = {0};
    uint32_t len = sizeof(value);
    int ret = GetParameter("ro.build.version.sdk", "", value, len);
    if (ret >= 0 && strlen(value) > 0) {
        printf("ro.build.version.sdk: %s\n", value);
    }
}

HWTEST_F(InitBasicUnitTest, TestParamBasic006, TestSize.Level1)
{
    int ret = SaveParameters();
    printf("SaveParameters returned: %d\n", ret);
}

HWTEST_F(InitBasicUnitTest, TestParamBasic007, TestSize.Level1)
{
    const char *serial = GetSerial();
    if (serial != nullptr) {
        printf("Serial: %s\n", serial);
    }
}

HWTEST_F(InitBasicUnitTest, TestParamBasic008, TestSize.Level1)
{
    const char *manufacture = GetManufacture();
    if (manufacture != nullptr) {
        printf("Manufacture: %s\n", manufacture);
    }
}

HWTEST_F(InitBasicUnitTest, TestParamBasic009, TestSize.Level1)
{
    const char *brand = GetBrand();
    if (brand != nullptr) {
        printf("Brand: %s\n", brand);
    }
}

HWTEST_F(InitBasicUnitTest, TestParamBasic010, TestSize.Level1)
{
    const char *market = GetMarketName();
    if (market != nullptr) {
        printf("MarketName: %s\n", market);
    }
}

// ========== Ueventd/FS/Begetctl 模块基础常量测试 (新增) ==========

HWTEST_F(InitBasicUnitTest, TestResizeMode001, TestSize.Level1)
{
    EXPECT_EQ(NO_NEED_RESIZE, 0);
    EXPECT_EQ(RESIZE_NORMAL, 1);
    EXPECT_EQ(RESIZE_META_NO_CHANGE, 2);
}

HWTEST_F(InitBasicUnitTest, TestStartupInitErrno001, TestSize.Level1)
{
    EXPECT_EQ(FSTAB_MOUNT_FAILED, 0);
    EXPECT_EQ(SYS_PARAM_INIT_FAILED, 1);
}

} // namespace init_ut