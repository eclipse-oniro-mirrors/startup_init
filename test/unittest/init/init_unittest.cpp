/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "init_unittest.h"

#include <cerrno>
#include <cstdio>
#include <ctime>

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include "init.h"
#include "device.h"
#include "init_cmds.h"
#include "init_log.h"
#include "init_service.h"
#include "init_adapter.h"
#include "init_utils.h"
#include "loop_event.h"
#include "param_stub.h"
#include "fs_manager/fs_manager.h"
#include "fd_holder.h"
#include "fd_holder_service.h"
#include "bootstage.h"
#include "parameter.h"
#include "fs_dm_linear.h"
#include "func_wrapper.h"
#include "init_group_manager.h"
#include "init_config_test_wrapper.h"
#include "cJSON.h"
#include <string>
#include <vector>

using namespace testing::ext;
using namespace std;

extern "C" {
INIT_STATIC void ProcessSignal(const struct signalfd_siginfo *siginfo);
INIT_STATIC void SetRootUid(void);
}

class BootModeEnvMock {
public:
    explicit BootModeEnvMock(const char *mode)
    {
        EnterBootModeMock(mode);
    }
    ~BootModeEnvMock()
    {
        ExitBootModeMock();
    }
    BootModeEnvMock(const BootModeEnvMock &) = delete;
    BootModeEnvMock &operator=(const BootModeEnvMock &) = delete;
};

namespace init_ut {
class InitUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

static int TestProcessSignal(struct signalfd_siginfo *siginfo)
{
    siginfo->ssi_signo = SIGCHLD;
    ProcessSignal(siginfo);
    siginfo->ssi_signo = SIGTERM;
    ProcessSignal(siginfo);
    siginfo->ssi_signo = SIGUSR1;
    ProcessSignal(siginfo);
    return 0;
}
static uid_t g_testUid = 999;
static int SetuidSuccessFunc(uid_t uid)
{
    g_testUid = uid;
    return 0;
}

static int SetuidFailFunc(uid_t uid)
{
    errno = EPERM;
    return -1;
}

HWTEST_F(InitUnitTest, TestSignalHandle, TestSize.Level1)
{
    struct signalfd_siginfo siginfo;
    int ret = TestProcessSignal(&siginfo);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(InitUnitTest, TestSystemPrepare, TestSize.Level1)
{
    SetStubResult(STUB_MOUNT, -1);
    EXPECT_EQ(GetStubResult(STUB_MOUNT), -1);
    SetStubResult(STUB_MKNODE, -1);
    EXPECT_EQ(GetStubResult(STUB_MKNODE), -1);
    CreateFsAndDeviceNode();

    SetStubResult(STUB_MOUNT, 0);
    EXPECT_EQ(GetStubResult(STUB_MOUNT), 0);
    SetStubResult(STUB_MKNODE, 0);
    EXPECT_EQ(GetStubResult(STUB_MKNODE), 0);
    CreateFsAndDeviceNode();
}

HWTEST_F(InitUnitTest, TestSystemExecRcs, TestSize.Level1)
{
    SystemExecuteRcs();
    Service *service = GetServiceByName("param_watcher");
    int ret = KeepCapability(service);
    EXPECT_EQ(ret, 0);
    ret = SetAmbientCapability(34); // CAP_SYSLOG
    EXPECT_EQ(ret, 0);
}

static void TestProcessTimer(const TimerHandle taskHandle, void *context)
{
    static int count = 0;
    printf("ProcessTimer %d\n", count);
    if (count == 0) { // 2 stop
        // set service pid for test
        Service *service = GetServiceByName("param_watcher");
        if (service != nullptr) {
            service->pid = getpid();
        }
        int fds1[] = {1, 0};
        ServiceSaveFd("param_watcher", fds1, ARRAY_LENGTH(fds1));
        ServiceSaveFdWithPoll("param_watcher", fds1, 0);
        ServiceSaveFdWithPoll("param_watcher", fds1, ARRAY_LENGTH(fds1));
        EXPECT_EQ(setenv("OHOS_FD_HOLD_param_watcher", "1 0", 0), 0);

        size_t fdCount = 0;
        int *fds = ServiceGetFd("param_watcher", &fdCount);
        EXPECT_TRUE(fds != nullptr);
        free(fds);

        ServiceSaveFd("testservice", fds1, ARRAY_LENGTH(fds1));
        ServiceSaveFd("deviceinfoservice", fds1, ARRAY_LENGTH(fds1));
    }
    if (count == 1) {
        LE_StopTimer(LE_GetDefaultLoop(), taskHandle);
        LE_StopLoop(LE_GetDefaultLoop());
    }
    count++;
}

HWTEST_F(InitUnitTest, TestFdHoldService, TestSize.Level1)
{
    RegisterFdHoldWatcher(-1);
    TimerHandle timer = nullptr;
    int ret = LE_CreateTimer(LE_GetDefaultLoop(), &timer, TestProcessTimer, nullptr);
    EXPECT_EQ(ret, 0);
    ret = LE_StartTimer(LE_GetDefaultLoop(), timer, 500, 4);
    EXPECT_EQ(ret, 0);
    SystemRun();
}

HWTEST_F(InitUnitTest, TestInitLog, TestSize.Level1)
{
    // test log
    CheckAndCreateDir(INIT_LOG_PATH);
    SetInitLogLevel(INIT_DEBUG);
    int ret = GetInitLogLevel();
    EXPECT_EQ(ret, INIT_DEBUG);
    INIT_LOGI("TestInitLog");
    INIT_LOGV("TestInitLog");
    INIT_LOGE("TestInitLog");
    INIT_LOGW("TestInitLog");
    INIT_LOGF("TestInitLog");
    // restore log level
    SetInitLogLevel(INIT_INFO);
    ret = GetInitLogLevel();
    EXPECT_EQ(ret, INIT_INFO);
}

HWTEST_F(InitUnitTest, TestParseCfgByPriority, TestSize.Level1)
{
    int ret = 0;
    ret = ParseCfgByPriority(NULL);
    EXPECT_EQ(ret, -1);
    ret = ParseCfgByPriority("etc/init");
    EXPECT_EQ(ret, 0);
    ret = ParseCfgByPriority("abc/abc");
    EXPECT_EQ(ret, -1);
}

HWTEST_F(InitUnitTest, TestReadConfigBopdMode, TestSize.Level1)
{
    BootModeEnvMock mock("bopd");
    MOCK_BOOT_MODE_CFG =
        "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
        "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},"
        "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bopd\"}]}]}";
    ReadConfig();
    ASSERT_EQ(g_mockCfgActionCount, 1);
    EXPECT_STREQ(g_mockCfgActions[0], "ParseCfgByPriority:etc/bopd");
}

HWTEST_F(InitUnitTest, TestReadConfigExistingBootModes, TestSize.Level1)
{
    {
        BootModeEnvMock mock("charger_mode");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"charger\",\"condition\":[{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.mode\",\"value\":\"charger_mode\"},{\"type\":\"misc\",\"value\":\"boot_charge\"}],"
            "\"actions\":[{\"action\":\"parse_default_cfg\",\"path\":\"/etc/init.cfg\"},"
            "{\"action\":\"read_dir\",\"path\":\"/system/etc/charger\"},"
            "{\"action\":\"parse_priority\",\"path\":\"etc/init\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 3);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseInitCfg:/etc/init.cfg");
        EXPECT_STREQ(g_mockCfgActions[1], "ReadFileInDir:/system/etc/charger");
        EXPECT_STREQ(g_mockCfgActions[2], "ParseCfgByPriority:etc/init");
    }
    {
        BootModeEnvMock mock("");
        MOCK_MISC_REASON = "boot_charge"; // misc 分区重启原因为 boot_charge
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"charger\",\"condition\":{\"type\":\"misc\",\"value\":\"boot_charge\"},"
            "\"actions\":[{\"action\":\"parse_default_cfg\"},"
            "{\"action\":\"read_dir\",\"path\":\"/system/etc/charger\"},"
            "{\"action\":\"parse_priority\",\"path\":\"etc/init\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 3);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseInitCfg:/etc/init.cfg");
        EXPECT_STREQ(g_mockCfgActions[1], "ReadFileInDir:/system/etc/charger");
        EXPECT_STREQ(g_mockCfgActions[2], "ParseCfgByPriority:etc/init");
    }
    {
        BootModeEnvMock mock("charger");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"charger\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.mode\",\"value\":\"charger\"},"
            "\"actions\":[{\"action\":\"read_dir\",\"path\":\"/system/etc/charger\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/system/etc/charger");
    }
    {
        BootModeEnvMock mock("");
        MockCmdlineParamSet("ohos.boot.minisys.mode", "penglai"); // cmdline 参数注入
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"penglai\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.minisys.mode\",\"value\":\"penglai\"},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/penglai\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseCfgByPriority:etc/penglai");
    }
    {
        BootModeEnvMock mock("");
        MockCmdlineParamSet("rescue_mode", "true"); // cmdline 参数注入
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"rescue\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"rescue_mode\",\"value\":\"true\"},"
            "\"actions\":[{\"action\":\"read_dir\",\"path\":\"/system/etc/rescue\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/system/etc/rescue");
    }
    {
        BootModeEnvMock mock("");
        MockCmdlineParamSet("ohos.boot.maintenance.recovery", "2");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"maintenance\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.maintenance.recovery\",\"value\":\"2\"},"
            "\"actions\":[{\"action\":\"read_dir\",\"path\":\"/system/etc/maintenance_recovery\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/system/etc/maintenance_recovery");
    }
    {
        BootModeEnvMock mock("");
        MockCmdlineParamSet("ohos.boot.maintenance.recovery", "3");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"maintenance\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.maintenance.recovery\",\"value\":\"3\"},"
            "\"actions\":[{\"action\":\"read_dir\",\"path\":\"/system/etc/maintenance_recovery\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/system/etc/maintenance_recovery");
    }
    {
        BootModeEnvMock mock("");
        g_mockUpdater = 1; // InUpdaterMode() != 0（存在 /bin/updater），命中 updater 条目
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"updater\",\"condition\":{\"type\":\"updater\"},"
            "\"actions\":[{\"action\":\"read_dir\",\"path\":\"/etc\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/etc");
    }
    {
        BootModeEnvMock mock(""); // 有 default 条目时仍会命中它 → 正常启动兜底动作
        g_mockUpdater = 0;        // 无 /bin/updater：updater 条件不成立，靠 default 兜底
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"default\",\"condition\":{\"type\":\"default\"},"
            "\"actions\":[{\"action\":\"parse_default_cfg\"},"
            "{\"action\":\"parse_priority\",\"path\":\"etc/init\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseInitCfg:/etc/init.cfg");
        EXPECT_STREQ(g_mockCfgActions[1], "ParseCfgByPriority:etc/init");
    }
}

HWTEST_F(InitUnitTest, TestReadConfigBootModePriority, TestSize.Level1)
{
    BootModeEnvMock mock("bopd");
    MockCmdlineParamSet("ohos.boot.minisys.mode", "penglai");
    g_mockUpdater = 0;
    MOCK_BOOT_MODE_CFG =
        "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
        "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},"
        "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bopd\"}]},"
        "{\"name\":\"penglai\",\"condition\":{\"type\":\"cmdline\","
        "\"param\":\"ohos.boot.minisys.mode\",\"value\":\"penglai\"},"
        "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/penglai\"}]},"
        "{\"name\":\"updater\",\"condition\":{\"type\":\"updater\"},"
        "\"actions\":[{\"action\":\"parse_default_cfg\"}]}]}";
    ReadConfig();
    ASSERT_EQ(g_mockCfgActionCount, 1);
    EXPECT_STREQ(g_mockCfgActions[0], "ParseCfgByPriority:etc/bopd");
}

HWTEST_F(InitUnitTest, TestBootModeCfgDrivenDispatch, TestSize.Level1)
{
    // 第一次：BOPD 条目动作指向 etc/bopd
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bopd\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseCfgByPriority:etc/bopd");
    }
    // 第二次：仅改配置内容（动作路径改为 etc/bopd2），序列应随配置改变
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bopd2\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseCfgByPriority:etc/bopd2");
    }
}

HWTEST_F(InitUnitTest, TestBootModeCfgNewModeNoSourceChange, TestSize.Level1)
{
    // 注入出厂配置之外的新模式（词表内组合：cmdline 条件 + 既有动作类型）
    BootModeEnvMock mock("mymode");
    MOCK_BOOT_MODE_CFG =
        "{\"boot_modes\":[{\"name\":\"mymode\",\"condition\":{\"type\":\"cmdline\","
        "\"param\":\"ohos.boot.mode\",\"value\":\"mymode\"},"
        "\"actions\":[{\"action\":\"read_dir\",\"path\":\"/system/etc/mymode\"},"
        "{\"action\":\"parse_default_cfg\"}]}]}";
    ReadConfig();
    ASSERT_EQ(g_mockCfgActionCount, 2);
    EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/system/etc/mymode");
    EXPECT_STREQ(g_mockCfgActions[1], "ParseInitCfg:/etc/init.cfg");
}

HWTEST_F(InitUnitTest, TestBootModeCfgMissingFallback, TestSize.Level1)
{
    // 配置缺失（MOCK_BOOT_MODE_CFG 为 NULL）：退回重构前的分支链
    // mock 环境下：无 charger_mode/misc/penglai/rescue/维修，InUpdaterMode=1 → else 分支
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG = NULL;
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/etc");
    }
    // 配置内容非法：同样退回重构前的分支链
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG = "{ not a valid json";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/etc");
    }
    // 配置可解析但无条目命中且无 default 条目：同样回退"正常启动"加载序列
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"penglai\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.minisys.mode\",\"value\":\"penglai\"},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/penglai\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseInitCfg:/etc/init.cfg");
        EXPECT_STREQ(g_mockCfgActions[1], "ParseCfgByPriority:etc/init");
    }
}

HWTEST_F(InitUnitTest, TestBootModeEmptyCfgFallback, TestSize.Level1)
{
    BootModeEnvMock mock("bopd");
    g_mockCfgLoaded = 0; // 模拟 etc/bopd 目录为空：所有加载动作都没有加载到 cfg
    MOCK_BOOT_MODE_CFG =
        "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
        "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},"
        "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bopd\"}],"
        "\"fallbackOnEmpty\":true}]}";
    ReadConfig();
    ASSERT_EQ(g_mockCfgActionCount, 3);
    EXPECT_STREQ(g_mockCfgActions[0], "ParseCfgByPriority:etc/bopd");
    EXPECT_STREQ(g_mockCfgActions[1], "ParseInitCfg:/etc/init.cfg");
    EXPECT_STREQ(g_mockCfgActions[2], "ParseCfgByPriority:etc/init");
}

HWTEST_F(InitUnitTest, TestBootModeEmptyCfgNoFallback, TestSize.Level1)
{
    BootModeEnvMock mock("charger");
    g_mockCfgLoaded = 0;             // 模拟 /system/etc/charger 目录为空
    MOCK_BOOT_MODE_CFG =
        "{\"boot_modes\":[{\"name\":\"charger\",\"condition\":{\"type\":\"cmdline\","
        "\"param\":\"ohos.boot.mode\",\"value\":\"charger\"},"
        "\"actions\":[{\"action\":\"read_dir\",\"path\":\"/system/etc/charger\"}]}]}";
    ReadConfig();
    ASSERT_EQ(g_mockCfgActionCount, 1);
    EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/system/etc/charger");
}

HWTEST_F(InitUnitTest, TestBootModeInvalidConditionFallback, TestSize.Level1)
{
    // 1) cmdline 条件缺 param/value
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"badCmdline\",\"condition\":{\"type\":\"cmdline\"},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bad\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseInitCfg:/etc/init.cfg");
        EXPECT_STREQ(g_mockCfgActions[1], "ParseCfgByPriority:etc/init");
    }
    // 2) 条件缺 type 字段
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"noType\",\"condition\":{},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bad\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
    }
    // 3) 条件类型未知
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"unknownCond\",\"condition\":{\"type\":\"unknownType\"},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bad\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
    }
    // 4) 条目缺 condition 字段
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"noCond\","
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bad\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
    }
}

HWTEST_F(InitUnitTest, TestBootModeInvalidActionSkipped, TestSize.Level1)
{
    // 1) 动作缺 action 字段
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},"
            "\"actions\":[{}],\"fallbackOnEmpty\":true}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseInitCfg:/etc/init.cfg");
    }
    // 2) read_dir 动作缺 path
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},"
            "\"actions\":[{\"action\":\"read_dir\"}],\"fallbackOnEmpty\":true}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
    }
    // 3) 动作类型未知
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},"
            "\"actions\":[{\"action\":\"unknownAction\",\"path\":\"etc/bad\"}],"
            "\"fallbackOnEmpty\":true}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
    }
}

HWTEST_F(InitUnitTest, TestBootModeCfgNoBootModes, TestSize.Level1)
{
    // 1) 完全缺少 boot_modes → 退回重构前的分支链
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG = "{\"other\":[]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/etc");
    }
    // 2) boot_modes 不是数组 → 同样退回原分支链
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG = "{\"boot_modes\":{}}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 1);
        EXPECT_STREQ(g_mockCfgActions[0], "ReadFileInDir:/etc");
    }
}

HWTEST_F(InitUnitTest, TestBootModeEntryNoActions, TestSize.Level1)
{
    // 1) 命中的条目没有 actions 字段
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"}}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseInitCfg:/etc/init.cfg");
    }
    // 2) actions 不是数组
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},\"actions\":{}}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
    }
}

HWTEST_F(InitUnitTest, TestGetMiscBootReasonInvalidArgs, TestSize.Level1)
{
    char reason[32] = {0};
    EXPECT_EQ(GetMiscBootReason(NULL, sizeof(reason)), -1);
    EXPECT_EQ(GetMiscBootReason(reason, 0), -1);
}

HWTEST_F(InitUnitTest, TestBootModeCondTypeMismatch, TestSize.Level1)
{
    // 1) param 不是字符串（!cJSON_IsString 分支）
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"badType\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":123,\"value\":\"true\"},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bad\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
        EXPECT_STREQ(g_mockCfgActions[0], "ParseInitCfg:/etc/init.cfg");
    }
    // 2) cmdline 条件有 param 无 value（value == NULL 分支）
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"noValue\",\"condition\":{\"type\":\"cmdline\","
            "\"param\":\"ohos.boot.mode\"},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bad\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
    }
    // 3) misc 条件缺 value（value == NULL 分支）
    {
        BootModeEnvMock mock("bopd");
        MOCK_BOOT_MODE_CFG =
            "{\"boot_modes\":[{\"name\":\"noMiscValue\",\"condition\":{\"type\":\"misc\"},"
            "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bad\"}]}]}";
        ReadConfig();
        ASSERT_EQ(g_mockCfgActionCount, 2);
    }
}

HWTEST_F(InitUnitTest, TestBootModeFallbackSkippedWhenCfgLoaded, TestSize.Level1)
{
    BootModeEnvMock mock("bopd");
    MOCK_BOOT_MODE_CFG =
        "{\"boot_modes\":[{\"name\":\"bopd\",\"condition\":{\"type\":\"cmdline\","
        "\"param\":\"ohos.boot.bopd.run.status\",\"value\":\"true\"},"
        "\"actions\":[{\"action\":\"parse_priority\",\"path\":\"etc/bopd\"}],"
        "\"fallbackOnEmpty\":true}]}";
    ReadConfig();
    ASSERT_EQ(g_mockCfgActionCount, 1);
    EXPECT_STREQ(g_mockCfgActions[0], "ParseCfgByPriority:etc/bopd");
}

namespace {
const char * const BOOT_MODE_CFG_DEVICE_PATH = "/system/etc/bootmode.cfg";

std::string ReadWholeFile(const std::string &path)
{
    FILE *fp = fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        return std::string();
    }
    std::string content;
    char buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        content.append(buf, n);
    }
    (void)fclose(fp);
    return content;
}

std::string ConditionToString(const cJSON *cond)
{
    const cJSON *type = cJSON_GetObjectItem(cond, "type");
    if (type == nullptr || !cJSON_IsString(type)) {
        return "<invalid-condition>";
    }
    std::string out = type->valuestring;
    const cJSON *param = cJSON_GetObjectItem(cond, "param");
    if (param != nullptr && cJSON_IsString(param)) {
        out += " param=" + std::string(param->valuestring);
    }
    const cJSON *value = cJSON_GetObjectItem(cond, "value");
    if (value != nullptr && cJSON_IsString(value)) {
        out += " value=" + std::string(value->valuestring);
    }
    if (cJSON_IsTrue(cJSON_GetObjectItem(cond, "exact"))) {
        out += " exact=true";
    }
    return out;
}

std::string EntryConditionsToString(const cJSON *entry)
{
    const cJSON *cond = cJSON_GetObjectItem(entry, "condition");
    if (cond == nullptr) {
        return "<no-condition>";
    }
    if (!cJSON_IsArray(cond)) {
        return ConditionToString(cond);
    }
    std::string out;
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, cond) {
        if (!out.empty()) {
            out += " OR ";
        }
        out += ConditionToString(item);
    }
    return out;
}

std::string EntryActionsToString(const cJSON *entry)
{
    const cJSON *acts = cJSON_GetObjectItem(entry, "actions");
    if (acts == nullptr || !cJSON_IsArray(acts)) {
        return "<no-actions>";
    }
    std::string out;
    const cJSON *act = nullptr;
    cJSON_ArrayForEach(act, acts) {
        const cJSON *type = cJSON_GetObjectItem(act, "action");
        const cJSON *path = cJSON_GetObjectItem(act, "path");
        if (!out.empty()) {
            out += " , ";
        }
        out += (type != nullptr && cJSON_IsString(type)) ? type->valuestring : "<invalid-action>";
        out += ":";
        out += (path != nullptr && cJSON_IsString(path)) ? path->valuestring : "<none>";
    }
    return out;
}
} // namespace

HWTEST_F(InitUnitTest, TestBootModeCfgFileContent, TestSize.Level1)
{
    struct ExpectedEntry {
        const char *name;
        const char *conditions;
        const char *actions;
        bool fallbackOnEmpty;
    };
    const std::vector<ExpectedEntry> expected = {
        { "charger_mode",
          "cmdline param=ohos.boot.mode value=charger_mode OR misc value=boot_charge",
          "parse_default_cfg:/etc/init.cfg , read_dir:/system/etc/charger , parse_priority:etc/init",
          false },
        { "bopd",
          "cmdline param=ohos.boot.bopd.run.status value=true",
          "parse_priority:etc/bopd",
          true },
        { "charger",
          "cmdline param=ohos.boot.mode value=charger",
          "read_dir:/system/etc/charger",
          false },
        { "penglai",
          "cmdline param=ohos.boot.minisys.mode value=penglai",
          "parse_priority:etc/penglai",
          false },
        { "rescue",
          "cmdline param=rescue_mode value=true",
          "read_dir:/system/etc/rescue",
          false },
        { "maintenance_recovery",
          "cmdline param=ohos.boot.maintenance.recovery value=2 exact=true OR "
          "cmdline param=ohos.boot.maintenance.recovery value=3 exact=true",
          "read_dir:/system/etc/maintenance_recovery",
          false },
        { "updater",
          "updater",
          "read_dir:/etc",
          false },
        { "default",
          "default",
          "parse_default_cfg:/etc/init.cfg , parse_priority:etc/init",
          false },
    };

    const std::string path = BOOT_MODE_CFG_DEVICE_PATH;
    const std::string content = ReadWholeFile(path);
    if (content.empty()) {
        GTEST_SKIP() << "读不到 " << path << "（该构建未安装出厂配置）";
    }

    cJSON *root = cJSON_Parse(content.c_str());
    ASSERT_NE(root, nullptr) << "bootmode.cfg 不是合法 JSON: " << path;
    cJSON *modes = cJSON_GetObjectItem(root, "boot_modes");
    ASSERT_NE(modes, nullptr);
    ASSERT_TRUE(cJSON_IsArray(modes));
    ASSERT_EQ(static_cast<size_t>(cJSON_GetArraySize(modes)), expected.size());

    for (size_t i = 0; i < expected.size(); i++) {
        cJSON *entry = cJSON_GetArrayItem(modes, static_cast<int>(i));
        ASSERT_NE(entry, nullptr) << "第 " << i << " 个条目缺失";
        const cJSON *name = cJSON_GetObjectItem(entry, "name");
        ASSERT_NE(name, nullptr);
        EXPECT_STREQ(name->valuestring, expected[i].name) << "第 " << i << " 个条目名不符";
        EXPECT_EQ(EntryConditionsToString(entry), std::string(expected[i].conditions))
            << "条目 " << expected[i].name << " 的条件不符";
        EXPECT_EQ(EntryActionsToString(entry), std::string(expected[i].actions))
            << "条目 " << expected[i].name << " 的动作不符";
        EXPECT_EQ(cJSON_IsTrue(cJSON_GetObjectItem(entry, "fallbackOnEmpty")), expected[i].fallbackOnEmpty)
            << "条目 " << expected[i].name << " 的 fallbackOnEmpty 不符";
    }
    cJSON_Delete(root);
}

HWTEST_F(InitUnitTest, FsDmCreateMultiTargetLinearDevice, TestSize.Level1)
{
    const char *devName = "testDevice";
    char dmDevPath[256] = "/dev/mapper/testDevice";
    uint64_t dmDevPathLen = sizeof(dmDevPath);

    DmLinearTarget targets[2];
    uint32_t targetNum = 2;

    targets[0].start = 0;
    targets[0].length = 100;
    targets[0].paras_len = strlen("linear /dev/sda 0") + 1;
    targets[0].paras = const_cast<char*>("linear /dev/sda 0");

    targets[1].start = 100;
    targets[1].length = 200;
    targets[1].paras_len = strlen("linear /dev/sdb 0") + 1;

    int fd = open("/dev/null", O_RDWR);
    int rc = FsDmCreateMultiTargetLinearDevice(devName, dmDevPath, dmDevPathLen, targets, targetNum);
    EXPECT_EQ(rc, -1);
    close(fd);
}

HWTEST_F(InitUnitTest, TestSetRootUid, TestSize.Level1)
{
    g_testUid = 999;
    UpdateSetuidFunc(SetuidFailFunc);
    SetRootUid();
    UpdateSetuidFunc(nullptr);

    UpdateSetuidFunc(SetuidSuccessFunc);
    SetRootUid();
    EXPECT_EQ(g_testUid, 0);
    UpdateSetuidFunc(nullptr);
}
}
