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

using namespace testing::ext;
using namespace std;

extern "C" {
INIT_STATIC void ProcessSignal(const struct signalfd_siginfo *siginfo);
int SwitchRoot(const char *newRoot)
{
    return 0;
}
}

namespace init_ut {
class InitUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(InitUnitTest, TestSignalHandle, TestSize.Level1)
{
    struct signalfd_siginfo siginfo;
    siginfo.ssi_signo = SIGCHLD;
    ProcessSignal(&siginfo);
    siginfo.ssi_signo = SIGTERM;
    ProcessSignal(&siginfo);
    siginfo.ssi_signo = SIGUSR1;
    ProcessSignal(&siginfo);
    SUCCEED();
}

HWTEST_F(InitUnitTest, TestSystemPrepare, TestSize.Level1)
{
    SetStubResult(STUB_MOUNT, -1);
    SetStubResult(STUB_MKNODE, -1);
    CreateFsAndDeviceNode();

    SetStubResult(STUB_MOUNT, 0);
    SetStubResult(STUB_MKNODE, 0);
    CreateFsAndDeviceNode();
}

HWTEST_F(InitUnitTest, TestSystemExecRcs, TestSize.Level1)
{
    SystemExecuteRcs();
    int ret = KeepCapability();
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
}

HWTEST_F(InitUnitTest, TestInitLog, TestSize.Level1)
{
    // test log
    CheckAndCreateDir(INIT_LOG_PATH);
    SetInitLogLevel(INIT_DEBUG);
    INIT_LOGI("TestInitLog");
    INIT_LOGV("TestInitLog");
    INIT_LOGE("TestInitLog");
    INIT_LOGW("TestInitLog");
    INIT_LOGF("TestInitLog");
    // restore log level
    int32_t loglevel = GetIntParameter("persist.init.debug.loglevel", INIT_ERROR);
    SetInitLogLevel((InitLogLevel)loglevel);
}
}
