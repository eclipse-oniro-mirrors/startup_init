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
#include "param_stub.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
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
    WriteParam("persist.init.bootevent.enable", "true", &dataIndex, 0);
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, nullptr, nullptr);
    // close switch for trace
    WriteParam("persist.init.bootevent.enable", "false", &dataIndex, 0);
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, nullptr, nullptr);
}

HWTEST_F(TraceUnitTest, TraceTest_002, TestSize.Level1)
{
    CreateInitTraceConfig(1);
    // start trace
    PluginExecCmdByName("init_trace", "start");
    // for run 1 s
    sleep(1);
    // stop trace
    PluginExecCmdByName("init_trace", "stop");
}

HWTEST_F(TraceUnitTest, TraceTest_003, TestSize.Level1)
{
    CreateInitTraceConfig(0);
    // start trace
    PluginExecCmdByName("init_trace", "start");
    // for run 1 s
    sleep(1);
    // stop trace
    PluginExecCmdByName("init_trace", "stop");
}

HWTEST_F(TraceUnitTest, TraceTest_004, TestSize.Level1)
{
    std::string cmdArgs = "/system/etc/init_trace.cfg  ";
    cmdArgs += STARTUP_INIT_UT_PATH"/system/etc/init_trace.cfg";
    int cmdIndex = 0;
    (void)GetMatchCmd("copy ", &cmdIndex);
    DoCmdByIndex(cmdIndex, cmdArgs.c_str(), nullptr);

    // start trace
    PluginExecCmdByName("init_trace", "start");
    // for run 1 s
    sleep(1);
    // stop trace
    PluginExecCmdByName("init_trace", "stop");
}

HWTEST_F(TraceUnitTest, TraceTest_005, TestSize.Level1)
{
    // start trace
    PluginExecCmdByName("init_trace", "start");
    // for run 1 s
    sleep(1);
    // interrupt trace
    PluginExecCmdByName("init_trace", "1");
}

HWTEST_F(TraceUnitTest, TraceTest_006, TestSize.Level1)
{
    std::string cmdArgs = "/bin/test    ";
    cmdArgs += STARTUP_INIT_UT_PATH"/bin/test";
    int cmdIndex = 0;
    (void)GetMatchCmd("copy ", &cmdIndex);
    DoCmdByIndex(cmdIndex, cmdArgs.c_str(), nullptr);

    // start trace
    PluginExecCmdByName("init_trace", "start");
    // for run 1 s
    sleep(1);
    // stop trace
    PluginExecCmdByName("init_trace", "stop");
}

HWTEST_F(TraceUnitTest, TraceTest_007, TestSize.Level1)
{
    CreateInitTraceConfig(0);
    // other case
    PluginExecCmdByName("init_trace", "other");
}
} // namespace init_ut
