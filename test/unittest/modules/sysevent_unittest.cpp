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
#include <cinttypes>
#include <ctime>
#include <sys/time.h>

#include "bootevent.h"
#include "parameter.h"
#include "init_param.h"
#include "init_utils.h"
#include "list.h"
#include "param_stub.h"
#include "securec.h"
#include "sys_event.h"
#include "init_cmdexecutor.h"

using namespace std;
using namespace testing::ext;
extern "C" {
extern void ReportBootEventComplete(ListNode *events);
}

static void AddBootEvent(ListNode *events, const char *name, int32_t type)
{
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)calloc(1, sizeof(BOOT_EVENT_PARAM_ITEM));
    if (item == nullptr) {
        return;
    }
    OH_ListInit(&item->node);
    item->paramName = strdup(name);
    if (item->paramName == nullptr) {
        free(item);
        return;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &(item->timestamp[BOOTEVENT_FORK]));
    (void)clock_gettime(CLOCK_MONOTONIC, &(item->timestamp[BOOTEVENT_READY]));
    item->flags = type;
    OH_ListAddTail(events, (ListNode *)&item->node);
}

static void BootEventDestroyProc(ListNode *node)
{
    if (node == nullptr) {
        return;
    }
    BOOT_EVENT_PARAM_ITEM *item = (BOOT_EVENT_PARAM_ITEM *)node;
    OH_ListRemove(node);
    OH_ListInit(node);
    free(item->paramName);
    free(item);
}

static int TestReportBootEventComplete(ListNode *events)
{
    ReportBootEventComplete(events);
    return 0;
}

static int TestReportSysEvent(const StartupEvent *event)
{
    ReportSysEvent(event);
    return 0;
}

namespace init_ut {
class SysEventUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(SysEventUnitTest, SysEventTest_001, TestSize.Level1)
{
    EXPECT_EQ(TestReportBootEventComplete(nullptr), 0);
}

HWTEST_F(SysEventUnitTest, SysEventTest_002, TestSize.Level1)
{
    ListNode events = { &events, &events };
    // empty event
    EXPECT_EQ(TestReportBootEventComplete(&events), 0);
}

HWTEST_F(SysEventUnitTest, SysEventTest_003, TestSize.Level1)
{
    ListNode events = { &events, &events };
    // create event and report
    AddBootEvent(&events, "bootevent.11111.xxxx", BOOTEVENT_TYPE_JOB);
    AddBootEvent(&events, "bootevent.22222222.xxxx", BOOTEVENT_TYPE_SERVICE);
    AddBootEvent(&events, "bootevent.33333333333.xxxx", BOOTEVENT_TYPE_SERVICE);
    AddBootEvent(&events, "bootevent.44444444444444", BOOTEVENT_TYPE_SERVICE);
    AddBootEvent(&events, "bootevent.44444444444444.6666666666.777777", BOOTEVENT_TYPE_SERVICE);
    SystemWriteParam("ohos.boot.bootreason", "-1");
    EXPECT_EQ(TestReportBootEventComplete(&events), 0);
    OH_ListRemoveAll(&events, BootEventDestroyProc);
}

HWTEST_F(SysEventUnitTest, SysEventTest_004, TestSize.Level1)
{
    struct timespec curr = {0};
    int ret = clock_gettime(CLOCK_MONOTONIC, &curr);
    ASSERT_EQ(ret, 0);
    StartupTimeEvent startupTime = {};
    startupTime.event.type = STARTUP_TIME;
    startupTime.totalTime = curr.tv_sec;
    startupTime.totalTime = startupTime.totalTime * MSECTONSEC;
    startupTime.totalTime += curr.tv_nsec / USTONSEC;
    startupTime.detailTime = const_cast<char *>("buffer");
    startupTime.reason = const_cast<char *>("");
    startupTime.firstStart = 1;
    EXPECT_EQ(TestReportSysEvent(&startupTime.event), 0);
}

HWTEST_F(SysEventUnitTest, SysEventTest_005, TestSize.Level1)
{
    struct timespec curr = {0};
    int ret = clock_gettime(CLOCK_MONOTONIC, &curr);
    ASSERT_EQ(ret, 0);
    StartupTimeEvent startupTime = {};
    startupTime.event.type = STARTUP_EVENT_MAX;
    startupTime.totalTime = curr.tv_sec;
    startupTime.totalTime = startupTime.totalTime * MSECTONSEC;
    startupTime.totalTime += curr.tv_nsec / USTONSEC;
    startupTime.detailTime = const_cast<char *>("buffer");
    startupTime.reason = const_cast<char *>("");
    startupTime.firstStart = 1;
    EXPECT_EQ(TestReportSysEvent(&startupTime.event), 0);
}

HWTEST_F(SysEventUnitTest, SysEventTest_006, TestSize.Level1)
{
    int ret = TestReportSysEvent(nullptr);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(SysEventUnitTest, SysEventTest_007, TestSize.Level1)
{
    const char *appfwkReady[] = {"bootevent.appfwk.ready"};
    int ret = PluginExecCmd("unset_bootevent", 1, appfwkReady);
    EXPECT_EQ(ret, 0);
    printf("SysEventTest_007:%d\n", ret);
}

HWTEST_F(SysEventUnitTest, SysEventTest_008, TestSize.Level1)
{
    char key1[] = "persist.startup.bootcount";
    char value1[32] = {0};
    char value2[32] = {0};
    char defValue1[] = "value of key not exist...";
    int ret = GetParameter(key1, defValue1, value1, 32);
    EXPECT_NE(ret, static_cast<int>(strlen(defValue1)));
    UpdateBootCount();
    ret = GetParameter(key1, defValue1, value2, 32);
    printf("value1: '%s'\n", value1);
    printf("value2: '%s'\n", value2);
    EXPECT_NE(0, strcmp(value1, value2));
}
} // namespace init_ut
