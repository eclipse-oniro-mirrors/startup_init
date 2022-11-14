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

#include "bootchart.h"
#include "bootstage.h"
#include "init_utils.h"
#include "param_stub.h"
#include "securec.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
extern "C" {
long long GetJiffies(void);
char *ReadFileToBuffer(const char *fileName, char *buffer, uint32_t bufferSize);
void BootchartLogHeader(void);
void BootchartLogFile(FILE *log, const char *procfile);
void BootchartLogProcessStat(FILE *log, pid_t pid);
void bootchartLogProcess(FILE *log);
void *BootchartThreadMain(void *data);
void BootchartDestory(void);
int DoBootchartStart(void);
int DoBootchartStop(void);
int DoBootchartCmd(int id, const char *name, int argc, const char **argv);
int BootchartInit(void);
void BootchartExit(void);
}

class ModulesUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(ModulesUnitTest, TestBootchartInit, TestSize.Level1)
{
    EXPECT_EQ(BootchartInit(), 0);
    EXPECT_NE(GetJiffies(), -1);
    EXPECT_NE(DoBootchartStart(), 1);
    EXPECT_EQ(DoBootchartStop(), 0);
    BootchartExit();
}

HWTEST_F(ModulesUnitTest, TestReadFileToBuffer, TestSize.Level1)
{
    const char *fileName = "ModulesTest";
    char buffer[MAX_BUFFER_LEN] = {0};
    EXPECT_EQ(ReadFileToBuffer(fileName, buffer, MAX_BUFFER_LEN), nullptr);
}

HWTEST_F(ModulesUnitTest, TestBootchartLogFile, TestSize.Level1)
{
    DoBootchartStart();
    FILE *log = fopen("/data/init_ut/ModulesTest.log", "w");
    BootchartLogFile(log, "/proc/stat");
    if (log != NULL) {
        (void)fflush(log);
        (void)fclose(log);
    }
}

HWTEST_F(ModulesUnitTest, TestBootchartLogProcessStat, TestSize.Level1)
{
    FILE *log = fopen("/data/init_ut/ModulesTest.log", "w");
    pid_t selfPid = getpid();
    BootchartLogProcessStat(log, selfPid);
    if (log != NULL) {
        (void)fflush(log);
        (void)fclose(log);
    }
}

HWTEST_F(ModulesUnitTest, TestbootchartLogProcess, TestSize.Level1)
{
    FILE *log = fopen("/data/init_ut/ModulesTest.log", "w");
    bootchartLogProcess(log);
    if (log != NULL) {
        (void)fflush(log);
        (void)fclose(log);
    }
}

HWTEST_F(ModulesUnitTest, TestDoBootchartCmd, TestSize.Level1)
{
    const char *argv1[] = { "start" };
    const char *argv2[] = { "stop" };
    EXPECT_NE(DoBootchartCmd(0, "bootchart", 1, argv1), 1);
    EXPECT_NE(DoBootchartCmd(0, "bootchart", 1, argv2), 1);
}

HWTEST_F(ModulesUnitTest, TestDoBootchartInsall, TestSize.Level1)
{
    TestSetParamCheckResult("ohos.servicectrl.", 0777, 0);
    SystemWriteParam("persist.init.bootchart.enabled", "1");
    SystemWriteParam("persist.init.debug.dump.trigger", "1");
    SystemWriteParam("persist.init.debug.loglevel", "6");
    SystemWriteParam("ohos.servicectrl.cmd", "setloglevel 10");
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, NULL, NULL);
}
} // namespace init_ut
