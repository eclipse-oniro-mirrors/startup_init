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
#include "init_param.h"
#include "init_utils.h"
#include "list.h"
#include "param_stub.h"
#include "securec.h"
#include "sys_event.h"

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
    ReportBootEventComplete(nullptr);
}

HWTEST_F(SysEventUnitTest, SysEventTest_002, TestSize.Level1)
{
    ListNode events = { &events, &events };
    // empty event
    ReportBootEventComplete(&events);
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
    ReportBootEventComplete(&events);
    OH_ListRemoveAll(&events, BootEventDestroyProc);
}

HWTEST_F(SysEventUnitTest, SysEventTest_004, TestSize.Level1)
{
    struct timespec curr = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &curr) != 0) {
        return;
    }
    StartupTimeEvent startupTime = {};
    startupTime.event.type = STARTUP_TIME;
    startupTime.totalTime = curr.tv_sec;
    startupTime.totalTime = startupTime.totalTime * MSECTONSEC;
    startupTime.totalTime += curr.tv_nsec / USTONSEC;
    startupTime.detailTime = const_cast<char *>("buffer");
    startupTime.reason = const_cast<char *>("");
    startupTime.firstStart = 1;
    ReportSysEvent(&startupTime.event);
}

HWTEST_F(SysEventUnitTest, SysEventTest_005, TestSize.Level1)
{
    struct timespec curr = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &curr) != 0) {
        return;
    }
    StartupTimeEvent startupTime = {};
    startupTime.event.type = STARTUP_EVENT_MAX;
    startupTime.totalTime = curr.tv_sec;
    startupTime.totalTime = startupTime.totalTime * MSECTONSEC;
    startupTime.totalTime += curr.tv_nsec / USTONSEC;
    startupTime.detailTime = const_cast<char *>("buffer");
    startupTime.reason = const_cast<char *>("");
    startupTime.firstStart = 1;
    ReportSysEvent(&startupTime.event);
}

HWTEST_F(SysEventUnitTest, SysEventTest_006, TestSize.Level1)
{
    ReportSysEvent(nullptr);
}
} // namespace init_ut
