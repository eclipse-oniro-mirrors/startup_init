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

// Unit tests for the deviceinfoservice SA env config path
// (SetEnvConfig/GetEnvConfig/RemoveEnvConfig). Self-contained and independent
// from the other five files under deviceinfo/ so that syncing the OH repo to
// the hmos repo (which only ships DeviceInfoUnittest.cpp) does not regress
// these cases.
//
// Mock strategy (matches DeviceInfoUnittest.cpp style): link-time symbol
// override. The test translation unit redefines the static IPCSkeleton and
// AccessTokenKit::GetHapTokenInfo methods used by the env path, driven by
// process-global state set per-case. -Dprivate=public (utest_config) exposes
// ResolveUserId / GetEnvConfigPath / LoadEnvConfig / SaveEnvConfig.

#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "accesstoken_kit.h"
#include "device_info_service.h"
#include "hap_token_info.h"
#include "ipc_skeleton.h"
#include "idevice_info.h"
#include "sysparam_errno.h"
#include "system_ability_definition.h"

using namespace testing::ext;
using namespace OHOS;

namespace {
// ---- Mock driving globals ----
// All defaults make the SA treat the caller as root with a derivable per-user
// token. Per-case tests override these before calling the SA.
uid_t g_callingUid = 0;
uint64_t g_callingFullTokenID = 0x12345;
pid_t g_callingPid = 0;
int g_getHapTokenInfoRet = 0;       // RET_SUCCESS
int32_t g_hapUserID = 0;            // populated token userID

// ---- Test infrastructure constants ----
// Mock defaults and test-infrastructure userId/mode. Centralized so the many
// call sites read by name instead of by raw literal.
constexpr pid_t MOCK_CALLING_PID = 1000;
constexpr int32_t TEST_USER_ID = 100;       // default userId for disk cases
constexpr mode_t MOCK_DIR_MODE = 0755;      // appenv dir mode (matches SA side)
} // namespace

// ---- Link-time mock of IPCSkeleton statics used by the env path ----
// Non-inline static methods; redefining here makes the linker prefer this TU's
// definition over the ipc_single shared library. Only the methods touched by
// device_info_service.cpp's env path are overridden; other IPCSkeleton uses in
// the SA (none in the env path) are unaffected.
namespace OHOS {
pid_t IPCSkeleton::GetCallingUid()
{
    return static_cast<pid_t>(g_callingUid);
}

pid_t IPCSkeleton::GetCallingPid()
{
    return g_callingPid;
}

uint64_t IPCSkeleton::GetCallingFullTokenID()
{
    return g_callingFullTokenID;
}
} // namespace OHOS

// ---- Link-time mock of AccessTokenKit::GetHapTokenInfo ----
// ResolveUserId only reads info.userID on success; set g_hapUserID per case.
namespace OHOS {
namespace Security {
namespace AccessToken {
int AccessTokenKit::GetHapTokenInfo(AccessTokenID tokenID, HapTokenInfo& info)
{
    (void)tokenID;
    if (g_getHapTokenInfoRet != 0) {
        return g_getHapTokenInfoRet;
    }
    info.userID = static_cast<int32_t>(g_hapUserID);
    return 0; // RET_SUCCESS
}
} // namespace AccessToken
} // namespace Security
} // namespace OHOS

namespace init_ut {
namespace {
// Per-user appenv.conf lives at /data/service/el1/<userId>/appenv/appenv.conf.
// The SA writes there with tmp+rename. On the device the directory is created
// by the appspawn PRE_FORK hook; under host ut we point the path at a tmp dir
// so file-content assertions work without root.
// The SA's GetEnvConfigPath is hardcoded to /data/service/el1/<id>/appenv/
// appenv.conf. The helpers below mirror that exact path so file-content
// assertions line up with what the SA actually wrote. The per-user directory
// under /data is normally created by the appspawn PRE_FORK hook; under host ut
// we create it on demand when the ut process can write there (root on device).

// Build the on-disk path the SA will compute for a given userId, mirroring
// DeviceInfoService::GetEnvConfigPath exactly (kept in sync by hand).
std::string AppenvPathFor(int32_t userId)
{
    return "/data/service/el1/" + std::to_string(userId) + "/appenv/appenv.conf";
}

// Ensure a directory exists with the given mode (mkdir -p + chmod).
void EnsureDir(const std::string& dir, mode_t mode)
{
    std::string cmd = "mkdir -p '" + dir + "'";
    (void)system(cmd.c_str());
    (void)chmod(dir.c_str(), mode);
}

// Reset every mock global to the "root + derivable userID=100" default so a
// forgotten override in one case cannot leak into the next.
void ResetMockDefaults()
{
    g_callingUid = 0;
    g_callingFullTokenID = 0x12345;
    g_callingPid = MOCK_CALLING_PID;
    g_getHapTokenInfoRet = 0;
    g_hapUserID = TEST_USER_ID;
}

// Read the whole appenv.conf for a user (empty string if missing).
std::string ReadAppenvConf(int32_t userId)
{
    std::string path = AppenvPathFor(userId);
    std::ifstream f(path);
    if (!f.is_open()) {
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return content;
}

// Pre-write an appenv.conf for a user (used by Get/Remove setups).
void WriteAppenvConf(int32_t userId, const std::string& content)
{
    std::string path = AppenvPathFor(userId);
    EnsureDir("/data/service/el1/" + std::to_string(userId) + "/appenv", MOCK_DIR_MODE);
    std::ofstream f(path, std::ios::trunc);
    f << content;
    f.flush();
    f.close();
}
} // namespace

// The SA's GetEnvConfigPath hardcodes /data/service/el1/<id>/appenv/appenv.conf.
// We cannot redirect that without source edits, so the host ut cases that assert
// on file content must run as root (utest_config uses --wrap=getuid). To keep
// these cases runnable by a non-root developer we instead assert on return
// codes and on the SA's in-memory behavior (LoadEnvConfig/SaveEnvConfig operate
// on whatever path the SA computes; when /data/service/el1 is not writable the
// SaveEnvConfig step returns ERR_FAIL, which the cases that need a file must
// tolerate). The path/validation/permission cases below do not depend on disk
// writes and are the primary coverage; the disk-write cases are guarded so they
// only assert file content when the path is actually writable.

class DeviceInfoAppenvUnitTest : public testing::Test {
protected:
    void SetUp() override
    {
        ResetMockDefaults();
        // Clean the per-user appenv dir used by the disk cases so they start
        // empty. Only TEST_USER_ID is used by these cases.
        EnsureDir("/data/service/el1/" + std::to_string(TEST_USER_ID) + "/appenv", MOCK_DIR_MODE);
        (void)unlink(AppenvPathFor(TEST_USER_ID).c_str());
    }
    void TearDown() override
    {
        (void)unlink(AppenvPathFor(TEST_USER_ID).c_str());
        ResetMockDefaults();
    }
    // True only if the SA's real path /data/service/el1/<userId>/appenv/ is
    // writable by this ut process (root on device). Used to gate file-content
    // assertions.
    static bool CanWriteRealAppenvPath(int32_t userId)
    {
        std::string dir = "/data/service/el1/" + std::to_string(userId) + "/appenv";
        if (access(dir.c_str(), W_OK) != 0) {
            return false;
        }
        return true;
    }
};

// ---------- invalid key/value format ----------

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_EmptyKey_InvalidInput, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string value;
    EXPECT_EQ(service.SetEnvConfig("", "v", 100), SYSPARAM_INVALID_INPUT);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_KeyStartsWithDigit_InvalidInput, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.SetEnvConfig("1KEY", "v", 100), SYSPARAM_INVALID_INPUT);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_KeyWithSpecialChar_InvalidInput, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.SetEnvConfig("A-B", "v", 100), SYSPARAM_INVALID_INPUT);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_KeyTooLong_InvalidInput, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string key(257, 'A');
    EXPECT_EQ(service.SetEnvConfig(key, "v", 100), SYSPARAM_INVALID_INPUT);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_ValueWithNewline_InvalidInput, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.SetEnvConfig("KEY", "a\nb", 100), SYSPARAM_INVALID_INPUT);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_ValueTooLong_InvalidInput, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string value(4097, 'x');
    EXPECT_EQ(service.SetEnvConfig("KEY", value, 100), SYSPARAM_INVALID_INPUT);
}

// ---------- key.empty() guards on Get/Remove ----------

HWTEST_F(DeviceInfoAppenvUnitTest, GetEnvConfig_EmptyKey_InvalidInput, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string out;
    EXPECT_EQ(service.GetEnvConfig("", out, 100), SYSPARAM_INVALID_INPUT);
}

HWTEST_F(DeviceInfoAppenvUnitTest, RemoveEnvConfig_EmptyKey_InvalidInput, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.RemoveEnvConfig("", 100), SYSPARAM_INVALID_INPUT);
}

// ---------- non-root caller rejected at CallbackEnter ----------

HWTEST_F(DeviceInfoAppenvUnitTest, CallbackEnter_EnvSet_NonRoot_Denied, TestSize.Level1)
{
    g_callingUid = 1000; // non-root
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    auto code = static_cast<uint32_t>(device_info::IDeviceInfoIpcCode::COMMAND_SET_ENV_CONFIG);
    EXPECT_EQ(service.CallbackEnter(code), SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoAppenvUnitTest, CallbackEnter_EnvGet_NonRoot_Denied, TestSize.Level1)
{
    g_callingUid = 1000;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    auto code = static_cast<uint32_t>(device_info::IDeviceInfoIpcCode::COMMAND_GET_ENV_CONFIG);
    EXPECT_EQ(service.CallbackEnter(code), SYSPARAM_PERMISSION_DENIED);
}

HWTEST_F(DeviceInfoAppenvUnitTest, CallbackEnter_EnvRemove_NonRoot_Denied, TestSize.Level1)
{
    g_callingUid = 1000;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    auto code = static_cast<uint32_t>(device_info::IDeviceInfoIpcCode::COMMAND_REMOVE_ENV_CONFIG);
    EXPECT_EQ(service.CallbackEnter(code), SYSPARAM_PERMISSION_DENIED);
}

// ---------- root caller passes CallbackEnter env gate ----------

HWTEST_F(DeviceInfoAppenvUnitTest, CallbackEnter_EnvSet_Root_Pass, TestSize.Level1)
{
    g_callingUid = 0;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    auto code = static_cast<uint32_t>(device_info::IDeviceInfoIpcCode::COMMAND_SET_ENV_CONFIG);
    EXPECT_EQ(service.CallbackEnter(code), 0);
}

HWTEST_F(DeviceInfoAppenvUnitTest, CallbackEnter_EnvGet_Root_Pass, TestSize.Level1)
{
    g_callingUid = 0;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    auto code = static_cast<uint32_t>(device_info::IDeviceInfoIpcCode::COMMAND_GET_ENV_CONFIG);
    EXPECT_EQ(service.CallbackEnter(code), 0);
}

HWTEST_F(DeviceInfoAppenvUnitTest, CallbackEnter_EnvRemove_Root_Pass, TestSize.Level1)
{
    g_callingUid = 0;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    auto code = static_cast<uint32_t>(device_info::IDeviceInfoIpcCode::COMMAND_REMOVE_ENV_CONFIG);
    EXPECT_EQ(service.CallbackEnter(code), 0);
}

// ---------- explicit --user out of range ----------

HWTEST_F(DeviceInfoAppenvUnitTest, ResolveUserId_ExplicitNegative_OutOfRange, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.ResolveUserId(-5), SYSPARAM_USERID_OUT_OF_RANGE);
}

HWTEST_F(DeviceInfoAppenvUnitTest, ResolveUserId_ExplicitOver999_OutOfRange, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.ResolveUserId(1000), SYSPARAM_USERID_OUT_OF_RANGE);
}

HWTEST_F(DeviceInfoAppenvUnitTest, ResolveUserId_ExplicitZero_OutOfRange, TestSize.Level1)
{
    // 0 is outside [100,999]; ResolveUserId returns USERID_OUT_OF_RANGE so
    // callers surface a distinct code from token-derivation failure
    // (SYSPARAM_INVALID_USERID, reserved for undeducible tokens).
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.ResolveUserId(0), SYSPARAM_USERID_OUT_OF_RANGE);
}

HWTEST_F(DeviceInfoAppenvUnitTest, ResolveUserId_ExplicitLow_OutOfRange, TestSize.Level1)
{
    // [1,99] are not valid OpenHarmony user ids; reject as out-of-range.
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.ResolveUserId(1), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.ResolveUserId(99), SYSPARAM_USERID_OUT_OF_RANGE);
}

HWTEST_F(DeviceInfoAppenvUnitTest, ResolveUserId_ExplicitValid_Passthrough, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.ResolveUserId(100), 100);
    EXPECT_EQ(service.ResolveUserId(999), 999);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_ExplicitOutOfRange_UserIdOutOfRange, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.SetEnvConfig("KEY", "v", -5), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.SetEnvConfig("KEY", "v", 99), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.SetEnvConfig("KEY", "v", 1000), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.SetEnvConfig("KEY", "v", 0), SYSPARAM_USERID_OUT_OF_RANGE);
}

HWTEST_F(DeviceInfoAppenvUnitTest, GetEnvConfig_ExplicitOutOfRange_UserIdOutOfRange, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string out;
    EXPECT_EQ(service.GetEnvConfig("KEY", out, -5), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.GetEnvConfig("KEY", out, 99), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.GetEnvConfig("KEY", out, 1000), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.GetEnvConfig("KEY", out, 0), SYSPARAM_USERID_OUT_OF_RANGE);
}

HWTEST_F(DeviceInfoAppenvUnitTest, RemoveEnvConfig_ExplicitOutOfRange_UserIdOutOfRange, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.RemoveEnvConfig("KEY", -5), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.RemoveEnvConfig("KEY", 99), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.RemoveEnvConfig("KEY", 1000), SYSPARAM_USERID_OUT_OF_RANGE);
    EXPECT_EQ(service.RemoveEnvConfig("KEY", 0), SYSPARAM_USERID_OUT_OF_RANGE);
}

// ---------- token-derived userID == 0 rejected ----------

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_TokenUserIdZero_InvalidUserid, TestSize.Level1)
{
    g_hapUserID = 0; // hdc default: token resolves but userID is 0
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.SetEnvConfig("KEY", "v", -1), SYSPARAM_INVALID_USERID);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_TokenResolveFail_InvalidUserid, TestSize.Level1)
{
    g_getHapTokenInfoRet = -1; // GetHapTokenInfo failure
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.SetEnvConfig("KEY", "v", -1), SYSPARAM_INVALID_USERID);
}

HWTEST_F(DeviceInfoAppenvUnitTest, GetEnvConfig_TokenUserIdZero_InvalidUserid, TestSize.Level1)
{
    g_hapUserID = 0;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string out;
    EXPECT_EQ(service.GetEnvConfig("KEY", out, -1), SYSPARAM_INVALID_USERID);
}

HWTEST_F(DeviceInfoAppenvUnitTest, RemoveEnvConfig_TokenUserIdZero_InvalidUserid, TestSize.Level1)
{
    g_hapUserID = 0;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.RemoveEnvConfig("KEY", -1), SYSPARAM_INVALID_USERID);
}

// ---------- explicit valid --user bypasses token derivation ----------

HWTEST_F(DeviceInfoAppenvUnitTest, ResolveUserId_ExplicitValue_NoTokenCall, TestSize.Level1)
{
    // If an explicit userId were routed through GetHapTokenInfo, a failing token
    // mock would turn a valid explicit value into INVALID_USERID. Drive the
    // token to fail and prove the explicit path ignores it.
    g_getHapTokenInfoRet = -1;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.ResolveUserId(100), 100);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_ExplicitValid_NotBlockedByFailedToken, TestSize.Level1)
{
    g_getHapTokenInfoRet = -1;
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    // Token failure must not short-circuit an explicit valid userId. Whether
    // the subsequent file write succeeds depends on /data writability; the only
    // hard guarantee here is that we do NOT get SYSPARAM_INVALID_USERID.
    int32_t ret = service.SetEnvConfig("KEY", "v", 100);
    EXPECT_NE(ret, SYSPARAM_INVALID_USERID);
}

// ---------- get missing key ----------

HWTEST_F(DeviceInfoAppenvUnitTest, GetEnvConfig_MissingKey_NotFound, TestSize.Level1)
{
    if (!CanWriteRealAppenvPath(100)) {
        GTEST_SKIP() << "requires writable /data/service/el1/100/appenv (run as root on device)";
    }
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::string out;
    EXPECT_EQ(service.GetEnvConfig("NOPE", out, 100), SYSPARAM_NOT_FOUND);
}

// ---------- remove missing vs existing ----------

HWTEST_F(DeviceInfoAppenvUnitTest, RemoveEnvConfig_MissingKey_NotFound, TestSize.Level1)
{
    if (!CanWriteRealAppenvPath(100)) {
        GTEST_SKIP() << "requires writable /data/service/el1/100/appenv (run as root on device)";
    }
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.RemoveEnvConfig("NOPE", 100), SYSPARAM_NOT_FOUND);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetThenGetThenRemove_RoundTrip, TestSize.Level1)
{
    if (!CanWriteRealAppenvPath(100)) {
        GTEST_SKIP() << "requires writable /data/service/el1/100/appenv (run as root on device)";
    }
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    ASSERT_EQ(service.SetEnvConfig("FOO", "bar", 100), 0);
    std::string out;
    EXPECT_EQ(service.GetEnvConfig("FOO", out, 100), 0);
    EXPECT_EQ(out, "bar");
    EXPECT_EQ(service.RemoveEnvConfig("FOO", 100), 0);
    EXPECT_EQ(service.GetEnvConfig("FOO", out, 100), SYSPARAM_NOT_FOUND);
    // file should no longer contain FOO
    std::string content = ReadAppenvConf(100);
    EXPECT_EQ(content.find("FOO=bar"), std::string::npos);
}

// ---------- set writes the file ----------

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_WritesAppenvConf, TestSize.Level1)
{
    if (!CanWriteRealAppenvPath(100)) {
        GTEST_SKIP() << "requires writable /data/service/el1/100/appenv (run as root on device)";
    }
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    ASSERT_EQ(service.SetEnvConfig("FOO", "bar", 100), 0);
    std::string content = ReadAppenvConf(100);
    EXPECT_NE(content.find("FOO=bar"), std::string::npos);
}

// ---------- 256-entry cap ----------

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_At256Entries_Full, TestSize.Level1)
{
    if (!CanWriteRealAppenvPath(100)) {
        GTEST_SKIP() << "requires writable /data/service/el1/100/appenv (run as root on device)";
    }
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    // Pre-fill 256 entries.
    for (int i = 0; i < 256; i++) {
        std::string k = "K" + std::to_string(i);
        ASSERT_EQ(service.SetEnvConfig(k, "v", 100), 0) << "prefill failed at " << i;
    }
    // 257th distinct key must be rejected.
    EXPECT_EQ(service.SetEnvConfig("NEWKEY", "v", 100), SYSPARAM_ENV_CONFIG_FULL);
}

HWTEST_F(DeviceInfoAppenvUnitTest, SetEnvConfig_OverwriteExisting_NotCounted, TestSize.Level1)
{
    if (!CanWriteRealAppenvPath(100)) {
        GTEST_SKIP() << "requires writable /data/service/el1/100/appenv (run as root on device)";
    }
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    ASSERT_EQ(service.SetEnvConfig("FOO", "v1", 100), 0);
    // Overwriting an existing key must not bump the entry count.
    EXPECT_EQ(service.SetEnvConfig("FOO", "v2", 100), 0);
    std::string out;
    EXPECT_EQ(service.GetEnvConfig("FOO", out, 100), 0);
    EXPECT_EQ(out, "v2");
}

// ---------- missing file treated as empty ----------

HWTEST_F(DeviceInfoAppenvUnitTest, LoadEnvConfig_FileNotExist_EmptySuccess, TestSize.Level1)
{
    if (!CanWriteRealAppenvPath(100)) {
        GTEST_SKIP() << "requires writable /data/service/el1/100/appenv (run as root on device)";
    }
    // Ensure the file does not exist for this user.
    std::string path = AppenvPathFor(100);
    (void)unlink(path.c_str());
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::map<std::string, std::string> cfg;
    EXPECT_EQ(service.LoadEnvConfig(cfg, 100), 0);
    EXPECT_TRUE(cfg.empty());
}

// ---------- corrupt lines skipped ----------

HWTEST_F(DeviceInfoAppenvUnitTest, LoadEnvConfig_CorruptLinesSkipped, TestSize.Level1)
{
    if (!CanWriteRealAppenvPath(100)) {
        GTEST_SKIP() << "requires writable /data/service/el1/100/appenv (run as root on device)";
    }
    std::string content =
        "GOOD=1\n"
        "# comment line\n"
        "no_equals_here\n"
        "=missingkey\n"
        "ALSO_GOOD=2\n"
        "TOOLONGKEY=" + std::string(257, 'x') + "\n";
    WriteAppenvConf(100, content);
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::map<std::string, std::string> cfg;
    EXPECT_EQ(service.LoadEnvConfig(cfg, 100), 0);
    EXPECT_EQ(cfg.size(), 2u);
    EXPECT_EQ(cfg["GOOD"], "1");
    EXPECT_EQ(cfg["ALSO_GOOD"], "2");
}

// ---------- SaveEnvConfig atomicity (tmp file not left behind) ----------

HWTEST_F(DeviceInfoAppenvUnitTest, SaveEnvConfig_TmpFileRemoved, TestSize.Level1)
{
    if (!CanWriteRealAppenvPath(100)) {
        GTEST_SKIP() << "requires writable /data/service/el1/100/appenv (run as root on device)";
    }
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    std::map<std::string, std::string> cfg = {{"K", "v"}};
    ASSERT_EQ(service.SaveEnvConfig(cfg, 100), 0);
    std::string tmp = AppenvPathFor(100) + ".tmp";
    struct stat st;
    EXPECT_NE(stat(tmp.c_str(), &st), 0) << ".tmp file left behind; rename not atomic";
}

// ---------- GetEnvConfigPath shape ----------

HWTEST_F(DeviceInfoAppenvUnitTest, GetEnvConfigPath_PerUserShape, TestSize.Level1)
{
    device_info::DeviceInfoService service(SYSPARAM_DEVICE_SERVICE_ID, false);
    EXPECT_EQ(service.GetEnvConfigPath(100), "/data/service/el1/100/appenv/appenv.conf");
    EXPECT_EQ(service.GetEnvConfigPath(999), "/data/service/el1/999/appenv/appenv.conf");
}
} // namespace init_ut
