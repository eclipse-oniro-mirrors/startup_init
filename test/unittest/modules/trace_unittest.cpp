/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "bootstage.h"
#include "init_utils.h"
#include "init_cmds.h"
#include "init_cmdexecutor.h"
#include "init_param.h"
#include "param_stub.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
/* Avoid name clash with INIT_LOCAL_API CheckParamValue in param_manager.h */
static void ExpectSystemParamEq(const char *name, const char *expectValue)
{
    char tmp[PARAM_BUFFER_SIZE] = {0};
    uint32_t len = sizeof(tmp);
    (void)SystemReadParam(name, tmp, &len);
    EXPECT_STREQ(tmp, expectValue) << "param " << name;
}

/* returns current value of a system param so cases can assert with a one-line EXPECT_STREQ */
static const char *UtParamValue(const char *name)
{
    static char tmp[PARAM_BUFFER_SIZE];
    if (memset_s(tmp, sizeof(tmp), 0, sizeof(tmp)) != EOK) {
        return "";
    }
    uint32_t len = sizeof(tmp) - 1;
    (void)SystemReadParam(name, tmp, &len);
    return tmp;
}

static bool IsConstDebuggableValue(const char *expectValue)
{
    char value[PARAM_BUFFER_SIZE] = {0};
    uint32_t len = sizeof(value);
    if (SystemReadParam("const.debuggable", value, &len) != 0 || len == 0) {
        return false;
    }
    return strcmp(value, expectValue) == 0;
}

static const char *g_content =
    "\"KERNEL\" : ["
    "{"
        "\"name\" : \"disk\","
        "\"description\" : \"Disk I/O\","
        "\"tag\" : 0,"
        "\"type\" : \"KERNEL\","
        "\"sys-files\" : ["
            "\"events/f2fs/f2fs_sync_file_enter/enable\","
            "\"events/f2fs/f2fs_sync_file_exit/enable\","
            "\"events/f2fs/f2fs_write_begin/enable\","
            "\"events/f2fs/f2fs_write_end/enable\","
            "\"events/ext4/ext4_da_write_begin/enable\","
            "\"events/ext4/ext4_da_write_end/enable\","
            "\"events/ext4/ext4_sync_file_enter/enable\","
            "\"events/ext4/ext4_sync_file_exit/enable\","
            "\"events/block/block_rq_issue/enable\","
            "\"events/block/block_rq_complete/enable\""
        "]"
    "},"
    "{"
        "\"name\" : \"mmc\","
        "\"description\" : \"eMMC commands\","
        "\"tag\" : 0,"
        "\"type\" : \"KERNEL\","
        "\"sys-files\" : ["
            "\"events/mmc/enable\""
        "]"
    "},"
    "{"
        "\"name\" : \"test\","
        "\"description\" : \"test\","
        "\"tag\" : 0,"
        "\"type\" : \"KERNEL\","
        "\"sys-files\" : ["
        "]"
    "}"
    "],"
    "\"USER\" : ["
        "{"
            "\"name\" : \"ohos\","
            "\"description\" : \"OpenHarmony\","
            "\"tag\" : 30,"
            "\"type\" : \"USER\","
            "\"sys-files\" : ["
            "]"
        "},"
        "{"
            "\"name\" : \"ability\","
            "\"description\" : \"Ability Manager\","
            "\"tag\" : 31,"
            "\"type\" : \"USER\","
            "\"sys-files\" : ["
            "]"
        "},"
        "{"
            "\"name\" : \"usb\","
            "\"description\" : \"usb subsystem\","
            "\"tag\" : 19,"
            "\"type\" : \"USER\","
            "\"sys-files\" : ["
            "]"
        "}"
    "]"
"}";
void CreateInitTraceConfig(int compress)
{
    std::string config = "{ \"compress\" : ";
    if (!compress) {
        config += "false,";
    } else {
        config += "true, ";
    }
    config += g_content;
    // create trace cfg
    CreateTestFile(STARTUP_INIT_UT_PATH"/system/etc/init_trace.cfg", config.c_str());
}

static int TestPluginExecCmdByName(const char *name, const char *cmdContent)
{
    PluginExecCmdByName(name, cmdContent);
    return 0;
}

class TraceUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(TraceUnitTest, TraceTest_001, TestSize.Level1)
{
    // open switch for trace
    uint32_t dataIndex = 0;
    int ret = WriteParam("persist.init.bootevent.enable", "true", &dataIndex, 0);
    EXPECT_EQ(ret, 0);
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, nullptr, nullptr);
    // close switch for trace
    ret = WriteParam("persist.init.bootevent.enable", "false", &dataIndex, 0);
    EXPECT_EQ(ret, 0);
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, nullptr, nullptr);
}

HWTEST_F(TraceUnitTest, TraceTest_002, TestSize.Level1)
{
    CreateInitTraceConfig(1);
    // start trace
    int ret = TestPluginExecCmdByName("init_trace", "start");
    EXPECT_EQ(ret, 0);
    // for run 1 s
    sleep(1);
    // stop trace
    ret = TestPluginExecCmdByName("init_trace", "stop");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TraceUnitTest, TraceTest_003, TestSize.Level1)
{
    CreateInitTraceConfig(0);
    // start trace
    int ret = TestPluginExecCmdByName("init_trace", "start");
    // for run 1 s
    sleep(1);
    // stop trace
    ret = TestPluginExecCmdByName("init_trace", "stop");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TraceUnitTest, TraceTest_004, TestSize.Level1)
{
    std::string cmdArgs = "/system/etc/init_trace.cfg  ";
    cmdArgs += STARTUP_INIT_UT_PATH"/system/etc/init_trace.cfg";
    int cmdIndex = 0;
    (void)GetMatchCmd("copy ", &cmdIndex);
    DoCmdByIndex(cmdIndex, cmdArgs.c_str(), nullptr);

    // start trace
    int ret = TestPluginExecCmdByName("init_trace", "start");
    EXPECT_EQ(ret, 0);
    // for run 1 s
    sleep(1);
    // stop trace
    ret = TestPluginExecCmdByName("init_trace", "stop");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TraceUnitTest, TraceTest_005, TestSize.Level1)
{
    // start trace
    int ret = TestPluginExecCmdByName("init_trace", "start");
    EXPECT_EQ(ret, 0);
    // for run 1 s
    sleep(1);
    // interrupt trace
    ret = TestPluginExecCmdByName("init_trace", "1");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TraceUnitTest, TraceTest_006, TestSize.Level1)
{
    std::string cmdArgs = "/bin/test    ";
    cmdArgs += STARTUP_INIT_UT_PATH"/bin/test";
    int cmdIndex = 0;
    (void)GetMatchCmd("copy ", &cmdIndex);
    DoCmdByIndex(cmdIndex, cmdArgs.c_str(), nullptr);

    // start trace
    int ret = TestPluginExecCmdByName("init_trace", "start");
    EXPECT_EQ(ret, 0);
    // for run 1 s
    sleep(1);
    // stop trace
    ret = TestPluginExecCmdByName("init_trace", "stop");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TraceUnitTest, TraceTest_007, TestSize.Level1)
{
    CreateInitTraceConfig(0);
    // other case
    int ret = TestPluginExecCmdByName("init_trace", "other");
    EXPECT_EQ(ret, 0);
}

HWTEST_F(TraceUnitTest, TraceTest_008_ActiveOnSkipsBootTrace, TestSize.Level1)
{
    uint32_t dataIndex = 0;
    if (!IsConstDebuggableValue("1")) {
        return;
    }
    EXPECT_EQ(WriteParam("persist.hitrace.boot_trace.count", "2", &dataIndex, 0), 0);
    EXPECT_EQ(WriteParam("debug.hitrace.boot_trace.active", "1", &dataIndex, 0), 0);

    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, nullptr, nullptr);

    ExpectSystemParamEq("persist.hitrace.boot_trace.count", "2");
    ExpectSystemParamEq("debug.hitrace.boot_trace.active", "1");
}

HWTEST_F(TraceUnitTest, TraceTest_009_InitTraceOnSkipsBootTrace, TestSize.Level1)
{
    uint32_t dataIndex = 0;
    if (!IsConstDebuggableValue("1")) {
        return;
    }
    EXPECT_EQ(WriteParam("persist.init.trace.enabled", "1", &dataIndex, 0), 0);
    EXPECT_EQ(WriteParam("persist.hitrace.boot_trace.count", "3", &dataIndex, 0), 0);
    EXPECT_EQ(WriteParam("debug.hitrace.boot_trace.active", "0", &dataIndex, 0), 0);

    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, nullptr, nullptr);

    ExpectSystemParamEq("persist.hitrace.boot_trace.count", "3");
    ExpectSystemParamEq("debug.hitrace.boot_trace.active", "0");
    (void)WriteParam("persist.init.trace.enabled", "0", &dataIndex, 0);
}

HWTEST_F(TraceUnitTest, TraceTest_010_DebuggableAndInactiveTriggersBootTrace, TestSize.Level1)
{
    uint32_t dataIndex = 0;
    if (!IsConstDebuggableValue("1")) {
        return;
    }
    EXPECT_EQ(WriteParam("persist.hitrace.boot_trace.count", "2", &dataIndex, 0), 0);
    EXPECT_EQ(WriteParam("debug.hitrace.boot_trace.active", "0", &dataIndex, 0), 0);

    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, nullptr, nullptr);

    // count is consumed once before spawn path when all trigger conditions are met.
    ExpectSystemParamEq("persist.hitrace.boot_trace.count", "1");
}

HWTEST_F(TraceUnitTest, TraceTest_011_NonDebuggableSkipsBootTrace, TestSize.Level1)
{
    uint32_t dataIndex = 0;
    if (!IsConstDebuggableValue("0")) {
        return;
    }
    EXPECT_EQ(WriteParam("persist.init.trace.enabled", "0", &dataIndex, 0), 0);
    EXPECT_EQ(WriteParam("persist.hitrace.boot_trace.count", "2", &dataIndex, 0), 0);
    EXPECT_EQ(WriteParam("debug.hitrace.boot_trace.active", "0", &dataIndex, 0), 0);

    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, nullptr, nullptr);

    ExpectSystemParamEq("persist.hitrace.boot_trace.count", "2");
    ExpectSystemParamEq("debug.hitrace.boot_trace.active", "0");
}

/* ---- extra coverage: state/arg validation and broken config ---- */
static void UtWriteParamSimple(const char *name, const char *value)
{
    uint32_t dataIndex = 0;
    EXPECT_EQ(WriteParam(name, value, &dataIndex, 0), 0);
}

/* invalid cmd dispatch: empty args, early stop/interrupt, unknown cmd; none changes trace state */
HWTEST_F(TraceUnitTest, TraceTest_012, TestSize.Level1)
{
    PluginExecCmdByName("init_trace", "");
    UtWriteParamSimple("persist.init.trace.enabled", "1");
    UtWriteParamSimple("debug.hitrace.boot_trace.active", "0");
    PluginExecCmdByName("init_trace", "stop");
    PluginExecCmdByName("init_trace", "1");
    PluginExecCmdByName("init_trace", "ut_unknown_cmd");
    EXPECT_STREQ(UtParamValue("persist.init.trace.enabled"), "1");
    EXPECT_STREQ(UtParamValue("debug.hitrace.boot_trace.active"), "0");
    UtWriteParamSimple("persist.init.trace.enabled", "0");
}

/* boot_trace with non-numeric count: count keeps unchanged */
HWTEST_F(TraceUnitTest, TraceTest_013_BootTraceInvalidCount, TestSize.Level1)
{
    UtWriteParamSimple("persist.init.trace.enabled", "0");
    UtWriteParamSimple("debug.hitrace.boot_trace.active", "0");
    UtWriteParamSimple("persist.hitrace.boot_trace.count", "ut-not-a-number");
    PluginExecCmdByName("init_trace", "boot_trace");
    EXPECT_STREQ(UtParamValue("persist.hitrace.boot_trace.count"), "ut-not-a-number");
    EXPECT_STREQ(UtParamValue("debug.hitrace.boot_trace.active"), "0");
}

/* boot_trace with count out of [1, 100]: count keeps unchanged */
HWTEST_F(TraceUnitTest, TraceTest_014_BootTraceCountOutOfRange, TestSize.Level1)
{
    UtWriteParamSimple("persist.init.trace.enabled", "0");
    UtWriteParamSimple("debug.hitrace.boot_trace.active", "0");
    UtWriteParamSimple("persist.hitrace.boot_trace.count", "0");
    PluginExecCmdByName("init_trace", "boot_trace");
    EXPECT_STREQ(UtParamValue("persist.hitrace.boot_trace.count"), "0");

    UtWriteParamSimple("persist.hitrace.boot_trace.count", "101");
    PluginExecCmdByName("init_trace", "boot_trace");
    EXPECT_STREQ(UtParamValue("persist.hitrace.boot_trace.count"), "101");
    EXPECT_STREQ(UtParamValue("debug.hitrace.boot_trace.active"), "0");
}

/* boot_trace when trace already active: count is not consumed */
HWTEST_F(TraceUnitTest, TraceTest_015_BootTraceAlreadyActive, TestSize.Level1)
{
    UtWriteParamSimple("persist.init.trace.enabled", "0");
    UtWriteParamSimple("persist.hitrace.boot_trace.count", "2");
    UtWriteParamSimple("debug.hitrace.boot_trace.active", "1");
    PluginExecCmdByName("init_trace", "boot_trace");
    EXPECT_STREQ(UtParamValue("persist.hitrace.boot_trace.count"), "2");
    EXPECT_STREQ(UtParamValue("debug.hitrace.boot_trace.active"), "1");
    UtWriteParamSimple("debug.hitrace.boot_trace.active", "0");
}

/* start with a broken (invalid json) config file: init trace workspace fails, config is left as is */
HWTEST_F(TraceUnitTest, TraceTest_016_BrokenConfig, TestSize.Level1)
{
    UtWriteParamSimple("persist.init.trace.enabled", "1");
    CreateTestFile(STARTUP_INIT_UT_PATH"/system/etc/init_trace.cfg", "{ not a valid json");
    PluginExecCmdByName("init_trace", "start");
    EXPECT_EQ(access(STARTUP_INIT_UT_PATH"/system/etc/init_trace.cfg", F_OK), 0);
    UtWriteParamSimple("persist.init.trace.enabled", "0");
    CreateInitTraceConfig(0);
}

/* start with config file missing: read file fails, start does not recreate the config */
HWTEST_F(TraceUnitTest, TraceTest_017_MissingConfig, TestSize.Level1)
{
    UtWriteParamSimple("persist.init.trace.enabled", "1");
    EXPECT_EQ(unlink(STARTUP_INIT_UT_PATH"/system/etc/init_trace.cfg"), 0);
    PluginExecCmdByName("init_trace", "start");
    EXPECT_NE(access(STARTUP_INIT_UT_PATH"/system/etc/init_trace.cfg", F_OK), 0);
    UtWriteParamSimple("persist.init.trace.enabled", "0");
    CreateInitTraceConfig(0);
}
} // namespace init_ut
