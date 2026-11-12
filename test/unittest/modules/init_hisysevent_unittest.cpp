/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <cstring>
#include <gtest/gtest.h>

#include "init_hisysevent.h"
#include "init_utils.h"
#include "securec.h"

#ifdef __cplusplus
extern "C" {
#endif
/* Stub accessors — defined in init_hisysevent_stub.cpp */
void ResetHiSysEventStub(void);
int32_t GetHiSysEventWriteCallCount(void);
const char *GetLastHiSysEventDomain(void);
const char *GetLastHiSysEventName(void);
void SetHiSysEventWriteRet(int ret);
/* Test-only hook — defined in init_hisysevent.c under STARTUP_INIT_TEST */
void ResetRateLimitState(void);
#ifdef __cplusplus
}
#endif

using namespace std;
using namespace testing::ext;

namespace init_ut {

class InitHisysEventUnitTest : public testing::Test {
protected:
    void SetUp() override
    {
        SetBootCompleted(true);
        ResetRateLimitState();
        ResetHiSysEventStub();
        SetHiSysEventWriteRet(0);
    }
    void TearDown() override
    {
        SetBootCompleted(false);
        ResetHiSysEventStub();
    }
};

/* ---- ReportStartupInitReport ---- */

// Normal path: OH_HiSysEvent_Write is called for each invocation
HWTEST_F(InitHisysEventUnitTest, ReportStartupInitReport_Normal, TestSize.Level1)
{
    ReportStartupInitReport(0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);
    EXPECT_STREQ(GetLastHiSysEventDomain(), "INIT");
    EXPECT_STREQ(GetLastHiSysEventName(), "STARTUP_INIT_REPORT");

    ReportStartupInitReport(100);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 2);

    ReportStartupInitReport(-1);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 3);
}

/* ---- ReportStartupReboot ---- */

// argv == NULL → early return, no HiSysEvent write
HWTEST_F(InitHisysEventUnitTest, ReportStartupReboot_NullArgv, TestSize.Level1)
{
    ReportStartupReboot(nullptr);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 0);
}

// Normal argv → strcpy_s success → OH_HiSysEvent_Write called
HWTEST_F(InitHisysEventUnitTest, ReportStartupReboot_Normal, TestSize.Level1)
{
    ReportStartupReboot("reboot");
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);
    EXPECT_STREQ(GetLastHiSysEventName(), "STARTUP_REBOOT");

    ReportStartupReboot("shutdown");
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 2);

    ReportStartupReboot("updater");
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 3);
}

// argv longer than 32 bytes → strcpy_s fails → early return, no write
HWTEST_F(InitHisysEventUnitTest, ReportStartupReboot_LongArgv, TestSize.Level1)
{
    char longArg[64] = {0};
    ASSERT_EQ(memset_s(longArg, sizeof(longArg), 'A', 63), 0);
    ReportStartupReboot(longArg);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 0);
}

/* ---- ReportServiceStart ---- */

// IsBootCompleted() == false → early return, no write
HWTEST_F(InitHisysEventUnitTest, ReportServiceStart_BootNotCompleted, TestSize.Level1)
{
    SetBootCompleted(false);
    char name[] = "svc_boot_not_completed";
    ReportServiceStart(name, 100, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 0);
}

// serviceName == NULL → early return after IsBootCompleted, no write
HWTEST_F(InitHisysEventUnitTest, ReportServiceStart_NullName, TestSize.Level1)
{
    ReportServiceStart(nullptr, 100, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 0);
}

// Valid name → CheckRateLimit (new service) → OH_HiSysEvent_Write called
HWTEST_F(InitHisysEventUnitTest, ReportServiceStart_Normal, TestSize.Level1)
{
    char name[] = "svc_start_normal";
    ReportServiceStart(name, 200, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);
    EXPECT_STREQ(GetLastHiSysEventName(), "PROCESS_START");
}

// Same service 4 times: all pass rate limit (count ≤ 4)
HWTEST_F(InitHisysEventUnitTest, ReportServiceStart_RateLimitMax, TestSize.Level1)
{
    char name[] = "svc_ratelimit_max";
    for (int i = 0; i < 4; i++) {
        ReportServiceStart(name, 300 + i, 0);
    }
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 4);

    // 5th call: count becomes 5 > 4, CheckRateLimit returns false, no write
    ReportServiceStart(name, 310, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 4);
}

// Different services are tracked independently by CheckRateLimit
HWTEST_F(InitHisysEventUnitTest, ReportServiceStart_DifferentServicesIndependent, TestSize.Level1)
{
    char name1[] = "svc_indep_a";
    char name2[] = "svc_indep_b";
    // Exhaust service A's rate limit
    for (int i = 0; i < 4; i++) {
        ReportServiceStart(name1, 400 + i, 0);
    }
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 4);

    ReportServiceStart(name1, 410, 0); // dropped
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 4);

    // Service B should still be allowed (new service, independent tracking)
    ReportServiceStart(name2, 420, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 5);
}

/* ---- ReportChildProcessExit ---- */

// IsBootCompleted() == false → early return, no write
HWTEST_F(InitHisysEventUnitTest, ReportChildProcessExit_BootNotCompleted, TestSize.Level1)
{
    SetBootCompleted(false);
    ReportChildProcessExit("svc_exit_boot", 500, 1, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 0);
}

// serviceName == NULL → early return, no write
HWTEST_F(InitHisysEventUnitTest, ReportChildProcessExit_NullName, TestSize.Level1)
{
    ReportChildProcessExit(nullptr, 500, 1, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 0);
}

// Valid name → CheckRateLimit (new service) → OH_HiSysEvent_Write called
HWTEST_F(InitHisysEventUnitTest, ReportChildProcessExit_Normal, TestSize.Level1)
{
    ReportChildProcessExit("svc_exit_normal", 600, 0, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);
    EXPECT_STREQ(GetLastHiSysEventName(), "CHILD_PROCESS_EXIT");
}

// Same service 4 times: all pass rate limit (count ≤ 4)
HWTEST_F(InitHisysEventUnitTest, ReportChildProcessExit_RateLimitMax, TestSize.Level1)
{
    const char *name = "svc_exit_ratelimit";
    for (int i = 0; i < 4; i++) {
        ReportChildProcessExit(name, 700 + i, i, 0);
    }
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 4);

    // 5th: count > MAX, dropped
    ReportChildProcessExit(name, 710, 4, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 4);
}

// serviceName > 32 bytes: truncated on both rate-limit and report paths.
// The event is reported (not dropped) and the truncated name is rate limited
// like any other service.
HWTEST_F(InitHisysEventUnitTest, ReportChildProcessExit_LongName, TestSize.Level1)
{
    const char *longName = "svc_exit_with_a_very_long_service_name_exceeding_32_bytes";
    ReportChildProcessExit(longName, 800, 0, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);
    EXPECT_STREQ(GetLastHiSysEventName(), "CHILD_PROCESS_EXIT");

    // Same long name again: the truncated entry is hit like any other service.
    // Calls 2~4 of a fresh window would pass, but the first call above already
    // used count=1, so of these 4 calls only 3 pass and the last is dropped.
    for (int i = 0; i < 4; i++) {
        ReportChildProcessExit(longName, 801 + i, i, 0);
    }
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 4); // 1 + 3, 5th dropped
}

/* ---- CheckRateLimit: slot recycling after table is full ---- */

// Fill all 256 slots, then verify a new service recycles the oldest slot (LRU)
// instead of failing open: the write still proceeds and the recycled entry is
// rate limited like a freshly tracked service.
HWTEST_F(InitHisysEventUnitTest, CheckRateLimit_SlotExhaustion, TestSize.Level2)
{
    constexpr int maxTrackedServices = 256; // keep in sync with init_hisysevent.c
    char name[32] = {0};
    int32_t countBefore = GetHiSysEventWriteCallCount();
    for (int i = 0; i < maxTrackedServices; i++) {
        ASSERT_GE(sprintf_s(name, sizeof(name), "svc_slot_%03d", i), 0);
        ReportServiceStart(name, 900 + i, 0);
    }
    int32_t countAfterFill = GetHiSysEventWriteCallCount();
    EXPECT_EQ(countAfterFill - countBefore, maxTrackedServices);

    // Table is full: the new service reuses the oldest slot (LRU) and the
    // write proceeds instead of being dropped or failing open untracked.
    char overflowName[] = "svc_slot_overflow";
    ReportServiceStart(overflowName, 999, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount() - countAfterFill, 1);

    // The recycled slot is under rate limiting: the recycle call above set
    // count=1, so of these 4 calls counts 2~4 pass (3 calls) and the 4th is dropped.
    for (int i = 0; i < 4; i++) {
        ReportServiceStart(overflowName, 998 + i, 0);
    }
    EXPECT_EQ(GetHiSysEventWriteCallCount() - countAfterFill, 4); // 1 + 3, 5th dropped
}

/* ---- ReportServiceStart with spawnMode variations ---- */

// spawnMode = NOT_SASPAWN (0) and IS_SASPAWN (1)
HWTEST_F(InitHisysEventUnitTest, ReportServiceStart_SpawnModes, TestSize.Level1)
{
    char name0[] = "svc_spawn_0";
    char name1[] = "svc_spawn_1";
    ReportServiceStart(name0, 1000, 0);  // NOT_SASPAWN
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);

    ReportServiceStart(name1, 1001, 1);  // IS_SASPAWN
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 2);
}

/* ---- ReportChildProcessExit with spawnMode variations ---- */

HWTEST_F(InitHisysEventUnitTest, ReportChildProcessExit_SpawnModes, TestSize.Level1)
{
    ReportChildProcessExit("svc_exit_spawn_0", 1100, 0, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);

    ReportChildProcessExit("svc_exit_spawn_1", 1101, 1, 1);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 2);
}

/* ---- Combined: ReportServiceStart then ReportChildProcessExit same service ---- */

// Both functions can be called for the same service name. The rate limit table
// is reset in SetUp(), so both calls are tracked independently per test case.
HWTEST_F(InitHisysEventUnitTest, Combined_StartAndExit_SameService, TestSize.Level1)
{
    const char *name = "svc_combined";
    ReportServiceStart(const_cast<char *>(name), 1200, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);

    ReportChildProcessExit(name, 1200, 0, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 2);
}

/* ---- OH_HiSysEvent_Write failure: ReportServiceStart ---- */

// Verify that a non-zero return from OH_HiSysEvent_Write does not crash
HWTEST_F(InitHisysEventUnitTest, ReportServiceStart_WriteFailure, TestSize.Level1)
{
    SetHiSysEventWriteRet(-1);
    char name[] = "svc_write_fail";
    ReportServiceStart(name, 1300, 0);
    // Write was called even though it returned failure
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);
}

/* ---- OH_HiSysEvent_Write failure: ReportStartupInitReport ---- */

// Covers PLUGIN_ONLY_LOG true-branch (ret != 0) for ReportStartupInitReport
HWTEST_F(InitHisysEventUnitTest, ReportStartupInitReport_WriteFailure, TestSize.Level1)
{
    SetHiSysEventWriteRet(-1);
    ReportStartupInitReport(999);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);
    EXPECT_STREQ(GetLastHiSysEventName(), "STARTUP_INIT_REPORT");
}

/* ---- OH_HiSysEvent_Write failure: ReportStartupReboot ---- */

// Covers PLUGIN_ONLY_LOG true-branch (ret != 0) for ReportStartupReboot
HWTEST_F(InitHisysEventUnitTest, ReportStartupReboot_WriteFailure, TestSize.Level1)
{
    SetHiSysEventWriteRet(-1);
    ReportStartupReboot("reboot_fail");
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);
    EXPECT_STREQ(GetLastHiSysEventName(), "STARTUP_REBOOT");
}

/* ---- OH_HiSysEvent_Write failure: ReportChildProcessExit ---- */

// Covers PLUGIN_ONLY_LOG true-branch (ret != 0) for ReportChildProcessExit
HWTEST_F(InitHisysEventUnitTest, ReportChildProcessExit_WriteFailure, TestSize.Level1)
{
    SetHiSysEventWriteRet(-1);
    char name[] = "svc_exit_fail";
    ReportChildProcessExit(name, 1400, 0, 0);
    EXPECT_EQ(GetHiSysEventWriteCallCount(), 1);
    EXPECT_STREQ(GetLastHiSysEventName(), "CHILD_PROCESS_EXIT");
}

} // namespace init_ut
