/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "init_unittest.h"

#include <cerrno>
#include <cstdarg>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <vector>

#include "func_wrapper.h"
#include "bootstage.h"
#include "hookmgr.h"
#include "init.h"
#include "init_cmds.h"
#include "init_group_manager.h"
#include "init_service.h"
#include "init/init_service_by_userid_test.h"
#include "init_service_manager.h"
#include "init_service_socket.h"
#include "param_stub.h"
#include "parameter.h"
#include "securec.h"
#include "init/service_control_test.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
extern "C" {
void ProcessSignal(const struct signalfd_siginfo *siginfo);
void ReportChildExitStatus(Service *service, const char *serviceName, pid_t pid, int procStat, bool isSaspawn);
}

namespace {
constexpr const char *TEST_SERVICE = "init_by_user_ut";
constexpr const char *TEST_PATH = "/data/init_by_user_ut";
constexpr int32_t USER_A = 401;
constexpr int32_t USER_B = 402;
constexpr size_t BY_USER_SERVICE_NAME_CONTENT_MAX = 53;
constexpr pid_t DEFAULT_TEST_FORK_PID = 12345;
constexpr int TEST_CRASH_COUNT = 3;
constexpr int TEST_CRASH_TIME = 60;
constexpr mode_t TEST_EXEC_MODE = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
constexpr mode_t TEST_PARAM_MODE = S_IRWXU | S_IRWXG | S_IRWXO;

pid_t g_nextForkPid = DEFAULT_TEST_FORK_PID;
bool g_statSuccess = true;
int g_strdupCallCount = 0;
int g_strdupFailAt = -1;
int g_callocCallCount = 0;
int g_callocFailAt = -1;
int g_waitParamResult = 0;
int g_setParamResult = 0;
int g_writeParamResult = 0;
int g_writeParamSecondResult = 0;
int g_writeParamCallCount = 0;
pid_t g_waitpidResult = 0;
int g_waitpidStatus = 0;
char g_lastWrittenParamName[128] = {};
char g_lastWrittenParamValue[128] = {};
char g_lastSetParamName[128] = {};
char g_lastSetParamValue[128] = {};
char g_lastWaitParamName[128] = {};
char g_lastWaitParamValue[128] = {};
int g_lastWaitTimeout = -1;
constexpr int ACCESS_TOKEN_TEST_FD = 81;
constexpr unsigned int ACCESS_TOKENID_SET_USERID_NR = 12;
constexpr unsigned int ACCESS_TOKENID_GET_USERID_NR = 13;
int g_accessTokenOpenResult = ACCESS_TOKEN_TEST_FD;
int g_accessTokenSetResult = 0;
int g_accessTokenGetResult = 0;
int g_accessTokenCloseResult = 0;
int g_accessTokenOpenCount = 0;
int g_accessTokenIoctlCount = 0;
int g_accessTokenCloseCount = 0;
int g_accessTokenLastFlags = 0;
int g_accessTokenLastFd = -1;
uint32_t g_accessTokenSetUserId = 0;
uint32_t g_accessTokenReadbackUserId = 0;
bool g_accessTokenUseCustomReadback = false;

pid_t ForkReturnPid()
{
    return g_nextForkPid;
}

pid_t ForkFail()
{
    errno = EAGAIN;
    return -1;
}

int StatMock(const char *pathname, struct stat *buf)
{
    if (!g_statSuccess) {
        errno = ENOENT;
        return -1;
    }
    if (buf != nullptr) {
        (void)memset_s(buf, sizeof(*buf), 0, sizeof(*buf));
        buf->st_mode = S_IFREG | TEST_EXEC_MODE;
    }
    (void)pathname;
    return 0;
}

char *StrdupMaybeFail(const char *string)
{
    g_strdupCallCount++;
    if (g_strdupCallCount == g_strdupFailAt) {
        errno = ENOMEM;
        return nullptr;
    }
    return __real_strdup(string);
}

void *CallocMaybeFail(size_t m, size_t n)
{
    g_callocCallCount++;
    if (g_callocCallCount == g_callocFailAt) {
        errno = ENOMEM;
        return nullptr;
    }
    return __real_calloc(m, n);
}

int AccessTokenOpenMock(const char *path, int flags)
{
    EXPECT_STREQ(path, "/dev/access_token_id");
    g_accessTokenOpenCount++;
    g_accessTokenLastFlags = flags;
    if (g_accessTokenOpenResult < 0) {
        errno = ENOENT;
    }
    return g_accessTokenOpenResult;
}

int AccessTokenIoctlMock(int fd, int request, va_list args)
{
    uint32_t *userId = va_arg(args, uint32_t *);
    g_accessTokenIoctlCount++;
    g_accessTokenLastFd = fd;
    EXPECT_NE(userId, nullptr);
    if (_IOC_TYPE(request) == 'A' && _IOC_NR(request) == ACCESS_TOKENID_SET_USERID_NR &&
        _IOC_DIR(request) == _IOC_WRITE) {
        if (g_accessTokenSetResult != 0) {
            errno = ENOTTY;
            return g_accessTokenSetResult;
        }
        g_accessTokenSetUserId = *userId;
        return 0;
    }
    if (_IOC_TYPE(request) == 'A' && _IOC_NR(request) == ACCESS_TOKENID_GET_USERID_NR &&
        _IOC_DIR(request) == _IOC_READ) {
        if (g_accessTokenGetResult != 0) {
            errno = ENOTTY;
            return g_accessTokenGetResult;
        }
        *userId = g_accessTokenUseCustomReadback ? g_accessTokenReadbackUserId : g_accessTokenSetUserId;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

int AccessTokenCloseMock(int fd)
{
    g_accessTokenCloseCount++;
    g_accessTokenLastFd = fd;
    return g_accessTokenCloseResult;
}

void ResetAccessTokenIoMock()
{
    g_accessTokenOpenResult = ACCESS_TOKEN_TEST_FD;
    g_accessTokenSetResult = 0;
    g_accessTokenGetResult = 0;
    g_accessTokenCloseResult = 0;
    g_accessTokenOpenCount = 0;
    g_accessTokenIoctlCount = 0;
    g_accessTokenCloseCount = 0;
    g_accessTokenLastFlags = 0;
    g_accessTokenLastFd = -1;
    g_accessTokenSetUserId = 0;
    g_accessTokenReadbackUserId = 0;
    g_accessTokenUseCustomReadback = false;
}

void ResetWrappers()
{
    UpdateForkFunc(nullptr);
    UpdateWaitpidFunc(nullptr);
    UpdateCallocFunc(nullptr);
    UpdateStrdupFunc(nullptr);
    UpdateOpenFunc(nullptr);
    UpdateIoctlFunc(nullptr);
    UpdateCloseFunc(nullptr);
    TestSetByUserStatFunc(nullptr);
    TestSetByUserWaitParamFunc(nullptr);
    TestSetByUserSetParamFunc(nullptr);
    TestSetByUserWriteParamFunc(nullptr);
    ResetAccessTokenIoMock();
    TestSetKillStubResult(0, 0);
    g_nextForkPid = DEFAULT_TEST_FORK_PID;
    g_statSuccess = true;
    g_strdupCallCount = 0;
    g_strdupFailAt = -1;
    g_callocCallCount = 0;
    g_callocFailAt = -1;
    g_waitParamResult = 0;
    g_setParamResult = 0;
    g_writeParamResult = 0;
    g_writeParamSecondResult = 0;
    g_writeParamCallCount = 0;
    g_waitpidResult = 0;
    g_waitpidStatus = 0;
    TestResetByUserReportSnapshot();
    g_lastWrittenParamName[0] = '\0';
    g_lastWrittenParamValue[0] = '\0';
    g_lastSetParamName[0] = '\0';
    g_lastSetParamValue[0] = '\0';
    g_lastWaitParamName[0] = '\0';
    g_lastWaitParamValue[0] = '\0';
    g_lastWaitTimeout = -1;
}

void CopyTestString(char *dest, size_t size, const char *source)
{
    if (dest == nullptr || size == 0 || source == nullptr) {
        return;
    }
    int ret = strcpy_s(dest, size, source);
    if (ret != EOK) {
        dest[0] = '\0';
        ADD_FAILURE() << "Failed to copy test string, ret=" << ret;
    }
}

int WaitParamMock(const char *name, const char *value, int waitTimeout)
{
    EXPECT_NE(name, nullptr);
    EXPECT_NE(value, nullptr);
    EXPECT_GE(waitTimeout, 0);
    if (name != nullptr) {
        CopyTestString(g_lastWaitParamName, sizeof(g_lastWaitParamName), name);
    }
    if (value != nullptr) {
        CopyTestString(g_lastWaitParamValue, sizeof(g_lastWaitParamValue), value);
    }
    g_lastWaitTimeout = waitTimeout;
    return g_waitParamResult;
}

int SetParamMock(const char *name, const char *value)
{
    EXPECT_NE(name, nullptr);
    EXPECT_NE(value, nullptr);
    if (name != nullptr) {
        CopyTestString(g_lastSetParamName, sizeof(g_lastSetParamName), name);
    }
    if (value != nullptr) {
        CopyTestString(g_lastSetParamValue, sizeof(g_lastSetParamValue), value);
    }
    return g_setParamResult;
}

int WriteParamMock(const char *name, const char *value)
{
    EXPECT_NE(name, nullptr);
    EXPECT_NE(value, nullptr);
    if (name != nullptr) {
        CopyTestString(g_lastWrittenParamName, sizeof(g_lastWrittenParamName), name);
    }
    if (value != nullptr) {
        CopyTestString(g_lastWrittenParamValue, sizeof(g_lastWrittenParamValue), value);
    }
    int ret = (g_writeParamCallCount == 0) ? g_writeParamResult : g_writeParamSecondResult;
    g_writeParamCallCount++;
    return ret;
}

pid_t WaitpidMock(pid_t pid, int *status, int options)
{
    EXPECT_EQ(pid, -1);
    EXPECT_EQ(options, WNOHANG);
    if (status != nullptr) {
        *status = g_waitpidStatus;
    }
    pid_t result = g_waitpidResult;
    g_waitpidResult = 0;
    return result;
}

void DoCmdByNameForByUser(const char *name, const char *cmdContent)
{
    int cmdIndex = 0;
    ASSERT_NE(GetMatchCmd(name, &cmdIndex), nullptr);
    DoCmdByIndex(cmdIndex, cmdContent, nullptr);
}

void ReleaseTestTemplateService()
{
    Service *service = GetServiceByName(TEST_SERVICE);
    if (service != nullptr) {
        ReleaseService(service);
    }
}

void InitServicePath(Service *service, const vector<const char *> &args)
{
    ASSERT_NE(service, nullptr);
    service->pathArgs.count = static_cast<int>(args.size());
    service->pathArgs.argv = static_cast<char **>(calloc(args.size() + 1, sizeof(char *)));
    ASSERT_NE(service->pathArgs.argv, nullptr);
    for (size_t i = 0; i < args.size(); i++) {
        service->pathArgs.argv[i] = strdup(args[i]);
        ASSERT_NE(service->pathArgs.argv[i], nullptr);
    }
    service->pathArgs.argv[args.size()] = nullptr;
    service->crashCount = TEST_CRASH_COUNT;
    service->crashTime = TEST_CRASH_TIME;
    service->pid = -1;
    service->status = SERVICE_IDLE;
    service->lastErrno = INIT_OK;
    service->attribute |= SERVICE_ATTR_ONDEMAND;
}

void ResetServicePath(Service *service)
{
    if (service == nullptr || service->pathArgs.argv == nullptr) {
        return;
    }
    for (int i = 0; i < service->pathArgs.count; i++) {
        free(service->pathArgs.argv[i]);
        service->pathArgs.argv[i] = nullptr;
    }
    free(service->pathArgs.argv);
    service->pathArgs.argv = nullptr;
    service->pathArgs.count = 0;
}

Service MakeStackService(const char *name = TEST_SERVICE)
{
    Service service {};
    service.name = const_cast<char *>(name);
    service.pid = -1;
    service.status = SERVICE_IDLE;
    service.lastErrno = INIT_OK;
    service.crashCount = TEST_CRASH_COUNT;
    service.crashTime = TEST_CRASH_TIME;
    OH_ListInit(&service.extDataNode);
    return service;
}
}

class ServiceByUserIdUnitTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        InitServiceSpace();
    }

    void SetUp() override
    {
        InitServiceSpace();
        ResetWrappers();
        TestResetServiceByUserIdInstances();
        TestSetParamCheckResult("ohos.servicectrl.", TEST_PARAM_MODE, 0);
        TestSetParamCheckResult("startup.service.ctl.", TEST_PARAM_MODE, 0);
    }

    void TearDown() override
    {
        ResetWrappers();
        TestResetServiceByUserIdInstances();
        ReleaseTestTemplateService();
    }
};

HWTEST_F(ServiceByUserIdUnitTest, InnerkitByUserCtrlValueAndActionMapping, TestSize.Level0)
{
    const char *extArgs[] = {"9004", "", "boot#event", "1"};
    TestSetByUserSetParamFunc(SetParamMock);
    g_setParamResult = 0;
    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, 100, extArgs, 4), 0);
    EXPECT_STREQ(g_lastSetParamName, "ohos.ctl.start.userid");
    EXPECT_STREQ(g_lastSetParamValue, "svc|100|9004||boot#event|1");

    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, 0, nullptr, 0), 0);
    EXPECT_STREQ(g_lastSetParamValue, "svc|0");

    EXPECT_EQ(ServiceControlWithExtraByUserId(nullptr, START, 100, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId("", START, 100, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId("bad|svc", START, 100, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId("bad svc", START, 100, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId("bad\tsvc", START, 100, extArgs, 1), -1);
    std::string overlongServiceName(BY_USER_SERVICE_NAME_CONTENT_MAX + 1, 's');
    EXPECT_EQ(ServiceControlWithExtraByUserId(overlongServiceName.c_str(), START, 100, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, -1, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, 100, nullptr, 1), -1);

    const char *tooMany[] = {"1", "2", "3", "4", "5"};
    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, 100, tooMany, 5), -1);
    const char *badExt[] = {"1|2"};
    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, 100, badExt, 1), -1);
    const char *blankExt[] = {"1 2"};
    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, 100, blankExt, 1), -1);
    const char *tabExt[] = {"1\t2"};
    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, 100, tabExt, 1), -1);
    const char *nullExt[] = {nullptr};
    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, 100, nullExt, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId("svc", START, 100, extArgs, -1), -1);
}

HWTEST_F(ServiceByUserIdUnitTest, InnerkitByUserCtrlValueLengthBoundary, TestSize.Level0)
{
    TestSetByUserSetParamFunc(SetParamMock);
    g_setParamResult = 0;
    for (size_t totalLen = 94; totalLen <= 97; totalLen++) {
        const size_t prefixLen = strlen("svc|100|");
        ASSERT_GT(totalLen, prefixLen);
        std::string extArg(totalLen - prefixLen, 'a');
        const char *extArgs[] = {extArg.c_str()};
        int ret = ServiceControlWithExtraByUserId("svc", START, 100, extArgs, 1);
        if (totalLen < 96) {
            EXPECT_EQ(ret, 0) << "totalLen=" << totalLen;
            EXPECT_EQ(strlen(g_lastSetParamValue), totalLen);
        } else {
            EXPECT_NE(ret, 0) << "totalLen=" << totalLen;
        }
    }
}

/*
 * @tc.name: BuildByUserCtrlValue_001
 * @tc.desc: Verify internal by-user control value validation and formatting failure branches.
 * @tc.type: FUNC
 */
HWTEST_F(ServiceByUserIdUnitTest, BuildByUserCtrlValue_001, TestSize.Level2)
{
    char value[PARAM_VALUE_LEN_MAX] = {};
    const char *extArgs[] = {"event"};
    EXPECT_NE(TestInnerkitHasByUserCtrlBlankChar(nullptr), 0);
    EXPECT_EQ(TestBuildByUserCtrlValue(value, sizeof(value), TEST_SERVICE, USER_A, extArgs, 1), 0);
    EXPECT_STREQ(value, "init_by_user_ut|401|event");

    EXPECT_NE(TestBuildByUserCtrlValue(nullptr, sizeof(value), TEST_SERVICE, USER_A, extArgs, 1), 0);
    EXPECT_NE(TestBuildByUserCtrlValue(value, 0, TEST_SERVICE, USER_A, extArgs, 1), 0);
    EXPECT_NE(TestBuildByUserCtrlValueWithNullArgs(value, sizeof(value)), 0);

    char tinyValue[4] = {};
    EXPECT_NE(TestBuildByUserCtrlValue(tinyValue, sizeof(tinyValue), TEST_SERVICE, USER_A, nullptr, 0), 0);
    EXPECT_NE(TestSetByUserProcessParamWithNullName(), 0);

    char paramName[PARAM_NAME_LEN_MAX] = {};
    char statusValue[MAX_INT_LEN] = {};
    EXPECT_NE(TestGetByUserProcessInfo(TEST_SERVICE, USER_A, nullptr, statusValue, SERVICE_STARTED), 0);
    EXPECT_NE(TestGetByUserProcessInfo(TEST_SERVICE, USER_A, paramName, nullptr, SERVICE_STARTED), 0);
}

HWTEST_F(ServiceByUserIdUnitTest, PublicByUserServiceControlWritesCtrlRequests, TestSize.Level0)
{
    const char *extArgs[] = {"9004", "", "boot_event"};
    TestSetByUserSetParamFunc(SetParamMock);
    g_setParamResult = 0;
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, START, USER_A, extArgs, 3), 0);
    EXPECT_STREQ(g_lastSetParamName, "ohos.ctl.start.userid");
    EXPECT_STREQ(g_lastSetParamValue, "init_by_user_ut|401|9004||boot_event");
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, STOP, USER_A, nullptr, 0), 0);
    EXPECT_STREQ(g_lastSetParamName, "ohos.ctl.stop.userid");
    EXPECT_STREQ(g_lastSetParamValue, "init_by_user_ut|401");
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, TERM, USER_A, nullptr, 0), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, RESTART, USER_A, extArgs, 1), -1);

    EXPECT_EQ(ServiceControlWithExtraByUserId(nullptr, START, USER_A, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId("", START, USER_A, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, START, -1, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, START, USER_A, nullptr, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, START, USER_A, extArgs, 5), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, SERVICE_ACTION_MAX, USER_A, extArgs, 1), -1);
    std::string overlongServiceName(BY_USER_SERVICE_NAME_CONTENT_MAX + 1, 's');
    EXPECT_EQ(ServiceControlWithExtraByUserId(overlongServiceName.c_str(), START, USER_A, extArgs, 1), -1);

    g_setParamResult = -1;
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, START, USER_A, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, STOP, USER_A, nullptr, 0), -1);
    TestSetByUserSetParamFunc(nullptr);
}

HWTEST_F(ServiceByUserIdUnitTest, PublicByUserWaitStatusUsesUserScopedKey, TestSize.Level0)
{
    TestSetByUserWaitParamFunc(WaitParamMock);

    g_waitParamResult = 0;
    EXPECT_EQ(ServiceWaitForStatusByUserId(TEST_SERVICE, USER_A, SERVICE_STARTED, 7), 0);
    EXPECT_STREQ(g_lastWaitParamName, "startup.service.ctl.init_by_user_ut.userid.401");
    EXPECT_STREQ(g_lastWaitParamValue, "2");
    EXPECT_EQ(g_lastWaitTimeout, 7);

    EXPECT_EQ(ServiceWaitForStatusByUserId(TEST_SERVICE, USER_B, SERVICE_READY, 0), 0);
    EXPECT_STREQ(g_lastWaitParamName, "startup.service.ctl.init_by_user_ut.userid.402");
    EXPECT_STREQ(g_lastWaitParamValue, "3");
    EXPECT_EQ(g_lastWaitTimeout, 0);

    g_waitParamResult = -1;
    EXPECT_EQ(ServiceWaitForStatusByUserId(TEST_SERVICE, USER_A, SERVICE_STOPPED, 1), -1);
    EXPECT_STREQ(g_lastWaitParamName, "startup.service.ctl.init_by_user_ut.userid.401");
    EXPECT_STREQ(g_lastWaitParamValue, "5");

    EXPECT_EQ(ServiceWaitForStatusByUserId(nullptr, USER_A, SERVICE_STOPPED, 1), -1);
    EXPECT_EQ(ServiceWaitForStatusByUserId("", USER_A, SERVICE_STOPPED, 1), -1);
    EXPECT_EQ(ServiceWaitForStatusByUserId("bad|svc", USER_A, SERVICE_STOPPED, 1), -1);
    EXPECT_EQ(ServiceWaitForStatusByUserId("bad svc", USER_A, SERVICE_STOPPED, 1), -1);
    EXPECT_EQ(ServiceWaitForStatusByUserId("bad\tsvc", USER_A, SERVICE_STOPPED, 1), -1);
    std::string overlongServiceName(BY_USER_SERVICE_NAME_CONTENT_MAX + 1, 's');
    EXPECT_EQ(ServiceWaitForStatusByUserId(overlongServiceName.c_str(), USER_A, SERVICE_STOPPED, 1), -1);
    EXPECT_EQ(ServiceWaitForStatusByUserId(TEST_SERVICE, -1, SERVICE_STOPPED, 1), -1);
    EXPECT_EQ(ServiceWaitForStatusByUserId(TEST_SERVICE, USER_A, SERVICE_STOPPED, -1), -1);

    std::string maxServiceName(BY_USER_SERVICE_NAME_CONTENT_MAX, 's');
    g_waitParamResult = 0;
    EXPECT_EQ(ServiceWaitForStatusByUserId(maxServiceName.c_str(), INT32_MAX, SERVICE_STOPPED, 3), 0);
    EXPECT_EQ(strlen(g_lastWaitParamName) + strlen(".pid"), static_cast<size_t>(PARAM_NAME_LEN_MAX - 1));
    EXPECT_STREQ(g_lastWaitParamValue, "5");
    EXPECT_EQ(g_lastWaitTimeout, 3);

    TestSetByUserWaitParamFunc(nullptr);
}

HWTEST_F(ServiceByUserIdUnitTest, PublicByUserRestartAndTermAreUnsupported, TestSize.Level0)
{
    const char *extArgs[] = {"after_restart"};
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, RESTART, USER_A, extArgs, 1), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, TERM, USER_A, nullptr, 0), -1);
    EXPECT_EQ(ServiceControlWithExtraByUserId(TEST_SERVICE, RESTART, -1, nullptr, 0), -1);
    EXPECT_EQ(ServiceWaitForStatusByUserId(nullptr, USER_A, SERVICE_STOPPED, 1), -1);
    EXPECT_EQ(ServiceWaitForStatusByUserId(TEST_SERVICE, -1, SERVICE_STOPPED, 1), -1);
    EXPECT_EQ(ServiceWaitForStatusByUserId(TEST_SERVICE, USER_A, SERVICE_STOPPED, -1), -1);
}

HWTEST_F(ServiceByUserIdUnitTest, ByUserCommandDispatchStartStop, TestSize.Level0)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH, "base"});
    TestSetByUserStatFunc(StatMock);
    UpdateForkFunc(ForkReturnPid);

    g_nextForkPid = 53001;
    DoCmdByNameForByUser("start_by_userid ", "init_by_user_ut|401|cmd");
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 53001);
    EXPECT_EQ(snapshot.status, SERVICE_STARTED);

    DoCmdByNameForByUser("stop_by_userid ", "init_by_user_ut|401");
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPING);
    EXPECT_EQ(snapshot.pid, 53001);

    ResetServicePath(service);
}

/*
 * @tc.name: DoStartByUserId_001
 * @tc.desc: Verify that the init job command accepts separate service name and user ID arguments.
 * @tc.type: FUNC
 */
HWTEST_F(ServiceByUserIdUnitTest, DoStartByUserId_001, TestSize.Level1)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH, "base"});
    TestSetByUserStatFunc(StatMock);
    UpdateForkFunc(ForkReturnPid);

    g_nextForkPid = 53002;
    DoCmdByNameForByUser("start_by_userid ", "init_by_user_ut 402");
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_B, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 53002);
    EXPECT_EQ(snapshot.status, SERVICE_STARTED);

    DoCmdByNameForByUser("stop_by_userid ", "init_by_user_ut|402");
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_B, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPING);
    EXPECT_EQ(snapshot.pid, 53002);

    ResetServicePath(service);
}

/*
 * @tc.name: DoStartByUserId_002
 * @tc.desc: Verify that invalid separate user ID arguments do not create by-user instances.
 * @tc.type: FUNC
 */
HWTEST_F(ServiceByUserIdUnitTest, DoStartByUserId_002, TestSize.Level2)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH, "base"});
    TestSetByUserStatFunc(StatMock);
    UpdateForkFunc(ForkReturnPid);

    DoCmdByNameForByUser("start_by_userid ", "init_by_user_ut bad_user");
    DoCmdByNameForByUser("start_by_userid ", "init_by_user_ut -1");
    DoCmdByNameForByUser("start_by_userid ", "init_by_user_ut 2147483648");
    EXPECT_EQ(TestGetServiceByUserIdCount(), 0);

    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, StartServiceByUserIdEncodedValueValidAndInvalid, TestSize.Level0)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH, "base"});
    TestSetByUserStatFunc(StatMock);
    UpdateForkFunc(ForkReturnPid);

    g_nextForkPid = 53101;
    StartServiceByUserId("init_by_user_ut|401|helper||tail");
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 53101);

    StartServiceByUserId(nullptr);
    StartServiceByUserId("");
    StartServiceByUserId("init_by_user_ut|-1");
    StartServiceByUserId("init_by_user_ut|bad_user");
    StartServiceByUserId("init_by_user_ut|402|1|2|3|4|5");

    EXPECT_EQ(TestGetServiceByUserIdCount(), 1);
    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, ParseByUserCtrlValueValidAndInvalid, TestSize.Level0)
{
    ByUserCtrlArgsTestSnapshot args {};
    EXPECT_EQ(TestParseByUserCtrlValue("svc|100", &args), 0);
    EXPECT_STREQ(args.serviceName, "svc");
    EXPECT_EQ(args.userId, 100);
    EXPECT_EQ(args.extArgc, 0);

    EXPECT_EQ(TestParseByUserCtrlValue("svc|101|arg1||arg3|arg4", &args), 0);
    EXPECT_STREQ(args.serviceName, "svc");
    EXPECT_EQ(args.userId, 101);
    EXPECT_EQ(args.extArgc, 4);
    EXPECT_STREQ(args.extArgs[0], "arg1");
    EXPECT_STREQ(args.extArgs[1], "");
    EXPECT_STREQ(args.extArgs[2], "arg3");
    EXPECT_STREQ(args.extArgs[3], "arg4");

    EXPECT_EQ(TestParseByUserCtrlValue("svc|102|", &args), 0);
    EXPECT_STREQ(args.serviceName, "svc");
    EXPECT_EQ(args.userId, 102);
    EXPECT_EQ(args.extArgc, 1);
    EXPECT_STREQ(args.extArgs[0], "");

    EXPECT_NE(TestParseByUserCtrlValue(nullptr, &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("", &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("|100", &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("bad svc|100", &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("svc|", &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("svc|-1", &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("svc|abc", &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("svc|2147483648", &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("svc|100|1|2|3|4|5", &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("svc|100|bad ext", &args), 0);
    EXPECT_NE(TestParseByUserCtrlValue("svc|100|bad\text", &args), 0);
    EXPECT_EQ(TestParseByUserCtrlValue("svc|100", nullptr), 0);

    std::string longValue(96, 'a');
    longValue += "|100";
    EXPECT_NE(TestParseByUserCtrlValue(longValue.c_str(), &args), 0);
    std::string longExt = "svc|100|";
    longExt.append(96, 'b');
    EXPECT_NE(TestParseByUserCtrlValue(longExt.c_str(), &args), 0);
}

HWTEST_F(ServiceByUserIdUnitTest, ParserPrimitiveInvalidBranches, TestSize.Level0)
{
    EXPECT_EQ(TestSplitNextToken(nullptr), nullptr);
    char *cursor = nullptr;
    EXPECT_EQ(TestSplitNextToken(&cursor), nullptr);

    char singleToken[] = "single";
    cursor = singleToken;
    EXPECT_STREQ(TestSplitNextToken(&cursor), "single");
    EXPECT_EQ(cursor, nullptr);

    char separatedTokens[] = "first|second";
    cursor = separatedTokens;
    EXPECT_STREQ(TestSplitNextToken(&cursor), "first");
    EXPECT_STREQ(cursor, "second");
    EXPECT_STREQ(TestSplitNextToken(&cursor), "second");
    EXPECT_EQ(cursor, nullptr);

    char token[8] = {};
    EXPECT_NE(TestCopyToken(nullptr, sizeof(token), "svc"), 0);
    EXPECT_NE(TestCopyToken(token, 0, "svc"), 0);
    EXPECT_NE(TestCopyToken(token, sizeof(token), nullptr), 0);
    EXPECT_NE(TestCopyToken(token, sizeof(token), ""), 0);
    EXPECT_NE(TestCopyToken(token, sizeof(token), "too-long-token"), 0);
    EXPECT_EQ(TestCopyToken(token, sizeof(token), "svc"), 0);
    EXPECT_STREQ(token, "svc");

    EXPECT_NE(TestCopyExtToken(nullptr, sizeof(token), "arg"), 0);
    EXPECT_NE(TestCopyExtToken(token, 0, "arg"), 0);
    EXPECT_NE(TestCopyExtToken(token, sizeof(token), nullptr), 0);
    EXPECT_NE(TestCopyExtToken(token, sizeof(token), "too-long-token"), 0);
    EXPECT_EQ(TestCopyExtToken(token, sizeof(token), ""), 0);
    EXPECT_STREQ(token, "");

    EXPECT_NE(TestHasByUserCtrlBlankChar(nullptr), 0);
    EXPECT_EQ(TestHasByUserCtrlBlankChar("service"), 0);
    EXPECT_EQ(TestHasByUserCtrlBlankChar("service name"), 1);

    int32_t userId = -1;
    EXPECT_NE(TestDecodeUserId(nullptr, &userId), 0);
    EXPECT_NE(TestDecodeUserId("", &userId), 0);
    EXPECT_NE(TestDecodeUserId("1", nullptr), 0);
    EXPECT_NE(TestDecodeUserId("999999999999999999999", &userId), 0);
    EXPECT_NE(TestDecodeUserId("abc", &userId), 0);
    EXPECT_NE(TestDecodeUserId("1x", &userId), 0);
    EXPECT_NE(TestDecodeUserId("-1", &userId), 0);
    EXPECT_NE(TestDecodeUserId("2147483648", &userId), 0);
    EXPECT_EQ(TestDecodeUserId("2147483647", &userId), 0);
    EXPECT_EQ(userId, INT32_MAX);
}

HWTEST_F(ServiceByUserIdUnitTest, KeyAndMcsBuilders, TestSize.Level0)
{
    char buffer[128] = {};
    EXPECT_EQ(TestBuildInstanceKey(buffer, sizeof(buffer), "svc", 100), 0);
    EXPECT_STREQ(buffer, "svc_u100");
    EXPECT_EQ(TestBuildByUserStatusKey(buffer, sizeof(buffer), "svc", 100), 0);
    EXPECT_STREQ(buffer, "startup.service.ctl.svc.userid.100");
    EXPECT_EQ(TestBuildByUserPidKey(buffer, sizeof(buffer), "svc", 100), 0);
    EXPECT_STREQ(buffer, "startup.service.ctl.svc.userid.100.pid");
    EXPECT_EQ(TestComputeMcsByUserId(100, buffer, sizeof(buffer)), 0);
    EXPECT_STREQ(buffer, "s0:c868");
    EXPECT_EQ(TestComputeMcsByUserId(256, buffer, sizeof(buffer)), 0);
    EXPECT_STREQ(buffer, "s0:c768");

    EXPECT_NE(TestBuildInstanceKey(nullptr, sizeof(buffer), "svc", 100), 0);
    EXPECT_NE(TestBuildInstanceKey(buffer, 0, "svc", 100), 0);
    EXPECT_NE(TestBuildInstanceKey(buffer, 4, "very_long_service", 100), 0);
    EXPECT_NE(TestBuildInstanceKey(buffer, sizeof(buffer), nullptr, 100), 0);
    EXPECT_NE(TestBuildByUserStatusKey(nullptr, sizeof(buffer), "svc", 100), 0);
    EXPECT_NE(TestBuildByUserStatusKey(buffer, 0, "svc", 100), 0);
    EXPECT_NE(TestBuildByUserStatusKey(buffer, sizeof(buffer), nullptr, 100), 0);
    EXPECT_NE(TestBuildByUserStatusKey(buffer, sizeof(buffer), "svc", -1), 0);
    EXPECT_NE(TestBuildByUserPidKey(nullptr, sizeof(buffer), "svc", 100), 0);
    EXPECT_NE(TestBuildByUserPidKey(buffer, 0, "svc", 100), 0);
    EXPECT_NE(TestBuildByUserPidKey(buffer, sizeof(buffer), nullptr, 100), 0);
    EXPECT_NE(TestBuildByUserPidKey(buffer, sizeof(buffer), "svc", -1), 0);
    EXPECT_NE(TestComputeMcsByUserId(-1, buffer, sizeof(buffer)), 0);
    EXPECT_NE(TestComputeMcsByUserId(100, nullptr, sizeof(buffer)), 0);
    EXPECT_NE(TestComputeMcsByUserId(100, buffer, 0), 0);

    char tiny[4] = {};
    EXPECT_NE(TestBuildByUserStatusKey(tiny, sizeof(tiny), "svc", 100), 0);
    EXPECT_NE(TestBuildByUserPidKey(tiny, sizeof(tiny), "svc", 100), 0);
    EXPECT_NE(TestComputeMcsByUserId(100, tiny, sizeof(tiny)), 0);
}

HWTEST_F(ServiceByUserIdUnitTest, RuntimePathArgsDeepCopyAndBounds, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH, "base"});
    ServiceArgs runtimeArgs {};
    ASSERT_EQ(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401|ext1|ext2", &runtimeArgs), 0);
    ASSERT_EQ(runtimeArgs.count, 4);
    EXPECT_STREQ(runtimeArgs.argv[0], TEST_PATH);
    EXPECT_STREQ(runtimeArgs.argv[1], "base");
    EXPECT_STREQ(runtimeArgs.argv[2], "ext1");
    EXPECT_STREQ(runtimeArgs.argv[3], "ext2");
    service.pathArgs.argv[1][0] = 'B';
    EXPECT_STREQ(runtimeArgs.argv[1], "base");
    TestFreeRuntimePathArgs(&runtimeArgs);

    ASSERT_EQ(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401|", &runtimeArgs), 0);
    ASSERT_EQ(runtimeArgs.count, 3);
    EXPECT_STREQ(runtimeArgs.argv[2], "");
    TestFreeRuntimePathArgs(&runtimeArgs);

    EXPECT_NE(TestBuildRuntimePathArgs(nullptr, "init_by_user_ut|401", &runtimeArgs), 0);
    EXPECT_NE(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401", nullptr), 0);
    EXPECT_NE(TestBuildRuntimePathArgsWithNullArgs(&service, &runtimeArgs), 0);
    service.pathArgs.count = 0;
    EXPECT_NE(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401", &runtimeArgs), 0);
    service.pathArgs.count = 2;
    char **savedArgv = service.pathArgs.argv;
    service.pathArgs.argv = nullptr;
    EXPECT_NE(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401", &runtimeArgs), 0);
    service.pathArgs.argv = savedArgv;
    char *savedExecutable = service.pathArgs.argv[0];
    service.pathArgs.argv[0] = nullptr;
    EXPECT_NE(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401", &runtimeArgs), 0);
    service.pathArgs.argv[0] = savedExecutable;
    TestFreeRuntimePathArgs(nullptr);
    ServiceArgs emptyArgs {};
    TestFreeRuntimePathArgs(&emptyArgs);
    EXPECT_EQ(emptyArgs.argv, nullptr);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, RuntimeServiceInitializationAndValidationBranches, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH, "base"});
    ServiceByUserId *instance = TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A);
    ASSERT_NE(instance, nullptr);

    EXPECT_NE(TestStartInstanceProcessWithNullInstance(), 0);
    EXPECT_NE(TestStartInstanceProcessWithNullArgs(instance), 0);
    EXPECT_NE(TestStartNullInstanceProcessWithPathArgs(&service.pathArgs), 0);

    Service runtimeService {};
    EXPECT_NE(TestInitRuntimeService(nullptr, instance, nullptr, 100), 0);
    EXPECT_NE(TestInitRuntimeService(&runtimeService, nullptr, nullptr, 100), 0);
    TestSetServiceByUserIdTemplate(TEST_SERVICE, USER_A, nullptr);
    EXPECT_NE(TestInitRuntimeService(&runtimeService, instance, nullptr, 100), 0);
    EXPECT_NE(TestStartInstanceProcessWithEmptyArgs(instance), 0);
    EXPECT_NE(TestStartInstanceProcessWithPathArgs(TEST_SERVICE, USER_A, &service.pathArgs), 0);
    TestSetServiceByUserIdTemplate(TEST_SERVICE, USER_A, &service);

    EXPECT_EQ(TestInitRuntimeService(&runtimeService, instance, nullptr, 101), 0);
    EXPECT_STREQ(runtimeService.name, "init_by_user_ut_u401");
    EXPECT_EQ(runtimeService.pid, 101);
    EXPECT_EQ(runtimeService.pathArgs.argv, service.pathArgs.argv);

    char overridePath[] = "/data/override";
    char *overrideArgv[] = {overridePath, nullptr};
    ServiceArgs overrideArgs {};
    overrideArgs.count = 1;
    overrideArgs.argv = overrideArgv;
    EXPECT_EQ(TestInitRuntimeService(&runtimeService, instance, &overrideArgs, 102), 0);
    EXPECT_EQ(runtimeService.pathArgs.argv, overrideArgv);
    EXPECT_EQ(runtimeService.pathArgs.count, 1);

    EXPECT_NE(TestIsByUserServiceInvalid(nullptr, &service, &service.pathArgs), 0);
    EXPECT_NE(TestIsByUserServiceInvalid(instance, nullptr, &service.pathArgs), 0);
    EXPECT_NE(TestIsByUserServiceInvalid(instance, &service, nullptr), 0);
    ServiceArgs emptyPath {};
    EXPECT_NE(TestIsByUserServiceInvalid(instance, &service, &emptyPath), 0);

    service.attribute |= SERVICE_ATTR_INVALID;
    EXPECT_NE(TestIsByUserServiceInvalid(instance, &service, &service.pathArgs), 0);
    service.attribute &= ~SERVICE_ATTR_INVALID;
    TestSetByUserStatFunc(StatMock);
    g_statSuccess = false;
    EXPECT_NE(TestIsByUserServiceInvalid(instance, &service, &service.pathArgs), 0);
    g_statSuccess = true;
    EXPECT_EQ(TestIsByUserServiceInvalid(instance, &service, &service.pathArgs), 0);

    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, RuntimePathArgsAllocationFailures, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH, "base"});
    ServiceArgs runtimeArgs {};

    UpdateCallocFunc(CallocMaybeFail);
    g_callocFailAt = 1;
    EXPECT_NE(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401|ext", &runtimeArgs), 0);
    EXPECT_EQ(runtimeArgs.argv, nullptr);
    EXPECT_EQ(runtimeArgs.count, 0);

    ResetWrappers();
    UpdateStrdupFunc(StrdupMaybeFail);
    g_strdupFailAt = 2;
    EXPECT_NE(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401|ext", &runtimeArgs), 0);
    EXPECT_EQ(runtimeArgs.argv, nullptr);
    EXPECT_EQ(runtimeArgs.count, 0);

    ResetWrappers();
    UpdateStrdupFunc(StrdupMaybeFail);
    g_strdupFailAt = 3;
    EXPECT_NE(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401|ext", &runtimeArgs), 0);
    EXPECT_EQ(runtimeArgs.argv, nullptr);
    EXPECT_EQ(runtimeArgs.count, 0);

    ResetWrappers();
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, AccessTokenUserIdIoctlSetGetAndFailureBranches, TestSize.Level0)
{
    Service service = MakeStackService();
    EXPECT_NE(SetUserIdToAccessToken(nullptr, USER_A), 0);
    service.name = nullptr;
    EXPECT_NE(SetUserIdToAccessToken(&service, USER_A), 0);
    service.name = const_cast<char *>(TEST_SERVICE);
    EXPECT_NE(SetUserIdToAccessToken(&service, -1), 0);
    EXPECT_EQ(g_accessTokenOpenCount, 0);

    UpdateOpenFunc(AccessTokenOpenMock);
    UpdateIoctlFunc(AccessTokenIoctlMock);
    UpdateCloseFunc(AccessTokenCloseMock);
    g_accessTokenOpenResult = -1;
    EXPECT_NE(SetUserIdToAccessToken(&service, USER_A), 0);
    EXPECT_EQ(g_accessTokenOpenCount, 1);
    EXPECT_EQ(g_accessTokenIoctlCount, 0);
    EXPECT_EQ(g_accessTokenCloseCount, 0);

    ResetAccessTokenIoMock();
    g_accessTokenSetResult = -1;
    EXPECT_NE(SetUserIdToAccessToken(&service, USER_A), 0);
    EXPECT_EQ(g_accessTokenIoctlCount, 1);
    EXPECT_EQ(g_accessTokenCloseCount, 1);

    ResetAccessTokenIoMock();
    g_accessTokenGetResult = -1;
    EXPECT_NE(SetUserIdToAccessToken(&service, USER_A), 0);
    EXPECT_EQ(g_accessTokenIoctlCount, 2);
    EXPECT_EQ(g_accessTokenCloseCount, 1);

    ResetAccessTokenIoMock();
    g_accessTokenUseCustomReadback = true;
    g_accessTokenReadbackUserId = USER_B;
    EXPECT_NE(SetUserIdToAccessToken(&service, USER_A), 0);
    EXPECT_EQ(g_accessTokenIoctlCount, 2);
    EXPECT_EQ(g_accessTokenCloseCount, 1);

    ResetAccessTokenIoMock();
    EXPECT_EQ(SetUserIdToAccessToken(&service, USER_A), 0);
    EXPECT_EQ(g_accessTokenOpenCount, 1);
    EXPECT_EQ(g_accessTokenIoctlCount, 2);
    EXPECT_EQ(g_accessTokenCloseCount, 1);
    EXPECT_EQ(g_accessTokenLastFlags, O_RDWR | O_CLOEXEC);
    EXPECT_EQ(g_accessTokenLastFd, ACCESS_TOKEN_TEST_FD);
    EXPECT_EQ(g_accessTokenSetUserId, static_cast<uint32_t>(USER_A));
}

HWTEST_F(ServiceByUserIdUnitTest, DirectStartMcsAndExplicitUserIdBranches, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH, "base"});
    ServiceByUserId *instance = TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A);
    ASSERT_NE(instance, nullptr);

    ServiceArgs runtimeArgs {};
    ASSERT_EQ(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401|ext", &runtimeArgs), 0);
    TestSetServiceByUserIdUser(TEST_SERVICE, USER_A, -1);
    EXPECT_EQ(TestStartInstanceProcessWithPathArgs(TEST_SERVICE, -1, &runtimeArgs), SERVICE_FAILURE);
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, -1, &snapshot), 0);
    EXPECT_EQ(snapshot.lastErrno, INIT_EPARAMETER);
    EXPECT_EQ(snapshot.status, SERVICE_IDLE);
    TestSetServiceByUserIdUser(TEST_SERVICE, -1, USER_A);

    TestSetByUserStatFunc(StatMock);
    UpdateForkFunc(ForkReturnPid);
    g_nextForkPid = 62001;
    EXPECT_EQ(TestStartInstanceProcessWithPathArgs(TEST_SERVICE, USER_A, &runtimeArgs), SERVICE_SUCCESS);
    instance = TestGetServiceByUserPid(62001);
    ASSERT_NE(instance, nullptr);

    TestFreeRuntimePathArgs(&runtimeArgs);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, ServiceByUserIdListUniquenessAndSnapshot, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    ServiceByUserIdTestSnapshot snapshot {};
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(nullptr, USER_A, &snapshot), -1);

    ServiceByUserId *first = TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A);
    ServiceByUserId *again = TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A);
    ServiceByUserId *secondUser = TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_B);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, again);
    ASSERT_NE(secondUser, nullptr);
    EXPECT_NE(first, secondUser);
    EXPECT_EQ(TestGetServiceByUserIdCount(), 2);

    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_STREQ(snapshot.serviceName, TEST_SERVICE);
    EXPECT_STREQ(snapshot.instanceKey, "init_by_user_ut_u401");
    EXPECT_EQ(snapshot.userId, USER_A);
    EXPECT_EQ(snapshot.pid, -1);
    EXPECT_EQ(snapshot.status, SERVICE_IDLE);
    EXPECT_EQ(snapshot.lastErrno, INIT_OK);
    EXPECT_EQ(snapshot.templateService, reinterpret_cast<uintptr_t>(&service));
    EXPECT_STREQ(snapshot.templateName, TEST_SERVICE);
    EXPECT_EQ(snapshot.templatePid, -1);
    EXPECT_EQ(snapshot.templateStatus, SERVICE_IDLE);

    service.name = nullptr;
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_STREQ(snapshot.templateName, "");
    service.name = const_cast<char *>(TEST_SERVICE);

    EXPECT_EQ(TestGetServiceByUserIdCount(), 2);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, ServiceByUserIdCreateAllocationFailures, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});

    EXPECT_EQ(TestGetOrCreateServiceByUserId(nullptr, TEST_SERVICE, USER_A), nullptr);
    EXPECT_EQ(TestGetOrCreateServiceByUserId(&service, nullptr, USER_A), nullptr);
    EXPECT_EQ(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, -1), nullptr);

    UpdateCallocFunc(CallocMaybeFail);
    g_callocFailAt = 1;
    EXPECT_EQ(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    EXPECT_EQ(TestGetServiceByUserIdCount(), 0);

    ResetWrappers();
    const char *tooLongName =
        "init_by_user_ut_name_is_far_longer_than_the_runtime_service_name_snapshot_buffer";
    EXPECT_EQ(TestGetOrCreateServiceByUserId(&service, tooLongName, USER_A), nullptr);
    EXPECT_EQ(TestGetServiceByUserIdCount(), 0);

    ResetWrappers();
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, TemplateReferenceAndRuntimeSnapshotDoNotPolluteTemplate, TestSize.Level0)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH, "base"});
    service->attribute |= SERVICE_ATTR_WITHOUT_SANDBOX;

    TestSetByUserStatFunc(StatMock);
    UpdateForkFunc(ForkReturnPid);
    g_nextForkPid = 23456;
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|401|ext"), SERVICE_SUCCESS);

    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.templateService, reinterpret_cast<uintptr_t>(service));
    EXPECT_STREQ(snapshot.templateName, TEST_SERVICE);
    EXPECT_EQ(snapshot.templatePid, -1);
    EXPECT_EQ(snapshot.templateStatus, SERVICE_IDLE);
    EXPECT_EQ(snapshot.templateAttribute, SERVICE_ATTR_ONDEMAND | SERVICE_ATTR_WITHOUT_SANDBOX);
    EXPECT_STREQ(snapshot.instanceKey, "init_by_user_ut_u401");
    EXPECT_STREQ(service->name, TEST_SERVICE);
    EXPECT_EQ(service->pid, -1);
    EXPECT_EQ(service->status, SERVICE_IDLE);
    ASSERT_NE(service->pathArgs.argv, nullptr);
    EXPECT_STREQ(service->pathArgs.argv[0], TEST_PATH);
    EXPECT_STREQ(service->pathArgs.argv[1], "base");
    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, ValidateByUserTemplateResourceFieldPolicy, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    EXPECT_EQ(TestValidateByUserTemplate(&service), 0);
    service.fdCount = 1;
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    service.fdCount = 0;
    int fd = 1;
    service.fds = &fd;
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    service.fds = nullptr;
    int timerValue = 1;
    service.timer = reinterpret_cast<TimerHandle>(&timerValue);
    EXPECT_EQ(TestValidateByUserTemplate(&service), 0);
    service.timer = nullptr;
    service.writePidArgs.count = 1;
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    service.writePidArgs.count = 0;
    CmdLines restartArg {};
    service.restartArg = &restartArg;
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    service.restartArg = nullptr;
    ServiceSocket socketCfg {};
    service.socketCfg = &socketCfg;
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    service.socketCfg = nullptr;
    ServiceFile fileCfg {};
    service.fileCfg = &fileCfg;
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    service.fileCfg = nullptr;
    EXPECT_NE(TestValidateByUserTemplate(nullptr), 0);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, ValidateByUserTemplateJobPolicy, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    service.serviceJobs.jobsName[JOB_PRE_START] = const_cast<char *>("test_job");
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    service.serviceJobs.jobsName[JOB_PRE_START] = nullptr;

    const int jobTypes[] = {JOB_ON_START, JOB_ON_STOP, JOB_ON_RESTART};
    for (int jobType : jobTypes) {
        service.serviceJobs.jobsName[jobType] = const_cast<char *>("test_job");
        EXPECT_EQ(TestValidateByUserTemplate(&service), 0) << "jobType=" << jobType;
        service.serviceJobs.jobsName[jobType] = nullptr;
    }
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, ValidateByUserTemplateAttributePolicy, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    service.attribute = 0;
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    service.attribute = SERVICE_ATTR_ONDEMAND | SERVICE_ATTR_IMPORTANT;
    EXPECT_EQ(TestValidateByUserTemplate(&service), 0);
    service.attribute = SERVICE_ATTR_ONDEMAND | SERVICE_ATTR_CRITICAL;
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    service.attribute = SERVICE_ATTR_ONDEMAND | SERVICE_ATTR_TIMERSTART;
    EXPECT_EQ(TestValidateByUserTemplate(&service), 0);
    service.attribute = SERVICE_ATTR_ONDEMAND | SERVICE_ATTR_PERIOD;
    EXPECT_NE(TestValidateByUserTemplate(&service), 0);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, UnsupportedTemplateDoesNotCreateInstance, TestSize.Level0)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH});
    service->attribute |= SERVICE_ATTR_PERIOD;

    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|401"), -1);
    EXPECT_EQ(TestGetServiceByUserIdCount(), 0);
    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, WriteByUserServiceStateValidation, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);

    TestSetByUserWriteParamFunc(WriteParamMock);
    g_writeParamResult = -7;
    EXPECT_EQ(TestWriteByUserServiceState(TEST_SERVICE, USER_A, SERVICE_STARTED), -7);
    g_writeParamCallCount = 0;
    g_writeParamResult = 0;
    g_writeParamSecondResult = -8;
    EXPECT_EQ(TestWriteByUserServiceState(TEST_SERVICE, USER_A, SERVICE_STARTED), -8);
    g_writeParamCallCount = 0;
    g_writeParamSecondResult = 0;
    EXPECT_EQ(TestWriteByUserServiceState(TEST_SERVICE, USER_A, SERVICE_STARTED), 0);
    EXPECT_EQ(TestWriteByUserServiceState(TEST_SERVICE, USER_B, SERVICE_STARTED), -1);
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_STARTED);

    TestSetServiceByUserIdUser(TEST_SERVICE, USER_A, -1);
    EXPECT_EQ(TestWriteByUserServiceState(TEST_SERVICE, -1, SERVICE_STOPPED), -1);
    TestSetServiceByUserIdUser(TEST_SERVICE, -1, USER_A);
    EXPECT_EQ(TestWriteNullServiceByUserState(), -1);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, StopServiceInstanceStateMachine, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    ServiceByUserIdTestSnapshot snapshot {};
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 2222, SERVICE_STARTED);
    EXPECT_EQ(TestStopServiceInstanceByUserId("init_by_user_ut|401"), SERVICE_SUCCESS);
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 2222);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPING);
    EXPECT_EQ(TestStopServiceInstanceByUserId("init_by_user_ut|401"), SERVICE_SUCCESS);
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPING);
    EXPECT_EQ(snapshot.pid, 2222);
    EXPECT_EQ(TestReapServiceByUserId(2222, 0), SERVICE_SUCCESS);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, -1, SERVICE_STARTED);
    EXPECT_EQ(TestStopServiceInstanceByUserId("init_by_user_ut|401"), SERVICE_SUCCESS);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 3333, SERVICE_STARTED);
    TestSetKillStubResult(-1, ESRCH);
    EXPECT_EQ(TestStopServiceInstanceByUserId("init_by_user_ut|401"), SERVICE_SUCCESS);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 3334, SERVICE_STARTED);
    TestSetKillStubResult(-1, EINVAL);
    EXPECT_EQ(TestStopServiceInstanceByUserId("init_by_user_ut|401"), SERVICE_FAILURE);
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 3334);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPING);
    TestSetKillStubResult(0, 0);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, ExternalEntryInvalidInputAndMissingInstanceDoNotCreateDirtyNodes, TestSize.Level0)
{
    TestStartServiceByUserId(nullptr);
    TestStartServiceByUserId("");
    TestStartServiceByUserId("bad");
    TestStartServiceByUserId("init_by_user_ut|bad_user");
    EXPECT_EQ(TestGetServiceByUserIdCount(), 0);
    EXPECT_EQ(TestStartNullServiceInstanceByUserId(), -1);
    EXPECT_EQ(TestStopNullServiceInstanceArgsByUserId(), -1);

    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH, "base"});
    TestStopServiceByUserId("init_by_user_ut|401");
    EXPECT_EQ(TestStopServiceInstanceByUserId("bad"), -1);
    EXPECT_EQ(TestStopServiceInstanceByUserId("init_by_user_ut|401"), -1);
    EXPECT_EQ(TestGetServiceByUserIdCount(), 0);

    TestStartServiceByUserId("missing_template|401");
    EXPECT_EQ(TestGetServiceByUserIdCount(), 0);
    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, StartInstanceParentPathAndForkFailure, TestSize.Level0)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH, "base"});

    TestSetByUserStatFunc(StatMock);
    UpdateForkFunc(ForkReturnPid);
    g_nextForkPid = 34567;
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|401|ext"), SERVICE_SUCCESS);
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 34567);
    EXPECT_EQ(snapshot.status, SERVICE_STARTED);
    EXPECT_STREQ(snapshot.mcs, "s0:c913");
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, -1, SERVICE_STOPPED);
    UpdateForkFunc(ForkFail);
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|401|again"), SERVICE_FAILURE);
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPED);
    EXPECT_EQ(snapshot.lastErrno, INIT_EFORK);

    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, StartInstanceUsesRealStatWhenNoTestHook, TestSize.Level0)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {"/data", "base"});
    UpdateForkFunc(ForkReturnPid);
    g_nextForkPid = 34601;

    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|401|real_stat"), SERVICE_SUCCESS);
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 34601);
    EXPECT_EQ(snapshot.status, SERVICE_STARTED);
    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, LegacyServiceStartForkFailureBranch, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {"/data"});
    UpdateForkFunc(ForkFail);

    EXPECT_EQ(ServiceStart(&service, &service.pathArgs), SERVICE_FAILURE);
    EXPECT_EQ(service.lastErrno, INIT_EFORK);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, RepeatedStartKeepsSingleRuntimeInstance, TestSize.Level0)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH, "base"});
    TestSetByUserStatFunc(StatMock);
    UpdateForkFunc(ForkReturnPid);

    g_nextForkPid = 44001;
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|401|first"), SERVICE_SUCCESS);
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 44001);
    EXPECT_EQ(TestGetServiceByUserIdCount(), 1);

    g_nextForkPid = 44002;
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|401|second"), SERVICE_SUCCESS);
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 44001);
    EXPECT_EQ(TestGetServiceByUserIdCount(), 1);
    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, StartRejectsInvalidPathAndInvalidTemplate, TestSize.Level0)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {"/not_exist/init_by_user_ut"});
    TestSetByUserStatFunc(StatMock);
    g_statSuccess = false;
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|401"), SERVICE_FAILURE);
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_IDLE);
    EXPECT_EQ(snapshot.lastErrno, INIT_EPATH);

    ResetServicePath(service);
    InitServicePath(service, {"/data/not_exist/init_by_user_ut"});
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|402"), SERVICE_FAILURE);
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_B, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_IDLE);
    EXPECT_EQ(snapshot.lastErrno, INIT_EPATH);

    service->attribute = 0;
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|403"), -1);

    service->attribute = SERVICE_ATTR_ONDEMAND | SERVICE_ATTR_INVALID;
    TestSetByUserStatFunc(StatMock);
    g_statSuccess = true;
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|404"), SERVICE_FAILURE);
    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, DirectStartWithPathArgsFailureBranches, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH, "base"});
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetByUserStatFunc(StatMock);

    EXPECT_EQ(TestStartInstanceProcessWithPathArgs(TEST_SERVICE, USER_A, nullptr), SERVICE_FAILURE);

    ServiceArgs emptyPath {};
    EXPECT_EQ(TestStartInstanceProcessWithPathArgs(TEST_SERVICE, USER_A, &emptyPath), SERVICE_FAILURE);

    ServiceArgs runtimeArgs {};
    ASSERT_EQ(TestBuildRuntimePathArgs(&service, "init_by_user_ut|401|ext", &runtimeArgs), 0);
    UpdateForkFunc(ForkReturnPid);
    g_nextForkPid = 55555;
    EXPECT_EQ(TestStartInstanceProcessWithPathArgs(TEST_SERVICE, USER_A, &runtimeArgs), SERVICE_SUCCESS);
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 55555);

    TestFreeRuntimePathArgs(&runtimeArgs);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, StopAllAndReapReleaseInstances, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_B), nullptr);

    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, -1, SERVICE_STOPPED);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_B, 45002, SERVICE_STARTED);
    EXPECT_EQ(TestGetServiceByUserIdCount(), 2);
    TestRemoveMissingServiceByUserIdInstance();
    EXPECT_EQ(TestGetServiceByUserIdCount(), 2);
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_B, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 45002);

    TestStopAllServiceByUserIdInstances();
    EXPECT_EQ(TestGetServiceByUserIdCount(), 1);
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_B, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPING);
    EXPECT_EQ(snapshot.pid, 45002);
    EXPECT_EQ(TestReapServiceByUserId(45002, 0), 0);
    EXPECT_NE(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_B, &snapshot), 0);

    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_B), nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 45001, SERVICE_STARTED);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_B, 45002, SERVICE_STARTED);
    TestStopAllServiceByUserIdInstances();
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPING);
    EXPECT_EQ(snapshot.pid, 45001);
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_B, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPING);
    EXPECT_EQ(snapshot.pid, 45002);
    EXPECT_EQ(TestReapServiceByUserId(45001, 0), 0);
    EXPECT_EQ(TestReapServiceByUserId(45002, 0), 0);
    EXPECT_EQ(TestGetServiceByUserIdCount(), 0);
    TestFreeNullServiceByUserIdInstance();
    TestRemoveNullServiceByUserIdInstance();
    EXPECT_EQ(TestStopNullServiceByUserIdInstance(), -1);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, StopInstanceWithMissingTemplate, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 46001, SERVICE_STARTED);
    TestSetServiceByUserIdTemplate(TEST_SERVICE, USER_A, nullptr);

    EXPECT_EQ(TestStopServiceInstanceByUserId("init_by_user_ut|401"), SERVICE_SUCCESS);
    ServiceByUserIdTestSnapshot snapshot {};
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.status, SERVICE_STOPPING);
    EXPECT_EQ(snapshot.pid, 46001);
    EXPECT_EQ(snapshot.templateService, 0U);

    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, GetServiceByUserPidMatchesCurrentPid, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 5100, SERVICE_STOPPING);

    EXPECT_EQ(TestGetServiceByUserPid(0), nullptr);
    EXPECT_EQ(TestGetServiceByUserPid(-1), nullptr);
    EXPECT_NE(TestGetServiceByUserPid(5100), nullptr);
    EXPECT_EQ(TestGetServiceByUserPid(5000), nullptr);
    EXPECT_EQ(TestGetServiceByUserPid(9999), nullptr);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, ProcessSignalReapsByUserServicePid, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 5200, SERVICE_STOPPING);

    g_waitpidResult = 5200;
    g_waitpidStatus = 0;
    UpdateWaitpidFunc(WaitpidMock);
    struct signalfd_siginfo siginfo {};
    siginfo.ssi_signo = SIGCHLD;
    siginfo.ssi_uid = 0;
    ProcessSignal(&siginfo);

    ServiceByUserIdTestSnapshot snapshot {};
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);
    UpdateWaitpidFunc(nullptr);
    ResetServicePath(&service);
}

/*
 * @tc.name: ReportChildExitStatus_001
 * @tc.desc: Verify normal and saspawn child exit reporting updates a valid service status.
 * @tc.type: FUNC
 */
HWTEST_F(ServiceByUserIdUnitTest, ReportChildExitStatus_001, TestSize.Level2)
{
    constexpr int normalExitCode = 7;
    constexpr int saspawnExitCode = 8;
    Service service = MakeStackService();

    ReportChildExitStatus(&service, TEST_SERVICE, 54001, normalExitCode << 8, false);
    EXPECT_EQ(service.lastErrno, normalExitCode);

    ReportChildExitStatus(&service, TEST_SERVICE, 54002, saspawnExitCode << 8, true);
    EXPECT_EQ(service.lastErrno, saspawnExitCode);
}

HWTEST_F(ServiceByUserIdUnitTest, ReapHandlesExitFailureOnceAndMissingTemplate, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    ServiceByUserIdTestSnapshot snapshot {};

    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 3000, SERVICE_STOPPING);
    EXPECT_EQ(TestReapServiceByUserId(3000, 0), 0);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 3001, SERVICE_STARTED);
    EXPECT_EQ(TestReapServiceByUserId(3001, 125 << 8), 0);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    service.attribute = SERVICE_ATTR_ONCE;
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 3002, SERVICE_STARTED);
    EXPECT_EQ(TestReapServiceByUserId(3002, 0), 0);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetServiceByUserIdTemplate(TEST_SERVICE, USER_A, nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 3003, SERVICE_STARTED);
    EXPECT_EQ(TestReapServiceByUserId(3003, 0), 0);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    EXPECT_EQ(TestReapServiceByUserId(9999, 0), -1);
    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, ReapExitStopsWithoutInitAutoRestartAndClearsStoppedPidParam, TestSize.Level0)
{
    Service *service = AddService(TEST_SERVICE);
    ASSERT_NE(service, nullptr);
    InitServicePath(service, {TEST_PATH, "base"});
    ASSERT_NE(TestGetOrCreateServiceByUserId(service, TEST_SERVICE, USER_A), nullptr);

    ServiceByUserIdTestSnapshot snapshot {};
    char pidKey[128] = {};
    ASSERT_EQ(TestBuildByUserPidKey(pidKey, sizeof(pidKey), TEST_SERVICE, USER_A), 0);
    TestSetByUserWriteParamFunc(WriteParamMock);

    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 7001, SERVICE_STARTED);
    EXPECT_EQ(TestReapServiceByUserId(7001, 0), 0);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);
    EXPECT_STREQ(g_lastWrittenParamName, pidKey);
    EXPECT_STREQ(g_lastWrittenParamValue, "0");
    TestSetByUserWriteParamFunc(nullptr);

    TestSetByUserStatFunc(StatMock);
    UpdateForkFunc(ForkReturnPid);
    g_nextForkPid = 7102;
    EXPECT_EQ(TestStartInstanceProcess("init_by_user_ut|401"), SERVICE_SUCCESS);
    ASSERT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), 0);
    EXPECT_EQ(snapshot.pid, 7102);

    EXPECT_EQ(TestReapServiceByUserId(7102, 0), 0);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    ResetServicePath(service);
}

HWTEST_F(ServiceByUserIdUnitTest, ReapSignalExitAndStaleCrashStateDoNotRestart, TestSize.Level0)
{
    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH, "base"});
    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);

    ServiceByUserIdTestSnapshot snapshot {};
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 8001, SERVICE_STARTED);
    EXPECT_EQ(TestReapServiceByUserId(8001, SIGTERM), 0);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    ASSERT_NE(TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A), nullptr);
    TestSetServiceByUserIdState(TEST_SERVICE, USER_A, 8002, SERVICE_STARTED);
    EXPECT_EQ(TestReapServiceByUserId(8002, 0), 0);
    EXPECT_EQ(TestFindServiceByUserIdSnapshot(TEST_SERVICE, USER_A, &snapshot), -1);

    ResetServicePath(&service);
}

HWTEST_F(ServiceByUserIdUnitTest, ReportServiceStartByUserInfoBranches, TestSize.Level0)
{
    ByUserReportTestSnapshot report {};
    TestReportServiceStartByUserInfo(nullptr, 1);
    TestGetByUserReportSnapshot(&report);
    EXPECT_EQ(report.serviceStartCount, 0);

    Service service = MakeStackService();
    InitServicePath(&service, {TEST_PATH});
    service.attribute = 0;
    TestReportServiceStartByUserInfo(&service, 1);
    TestGetByUserReportSnapshot(&report);
    EXPECT_EQ(report.serviceStartCount, 1);
    EXPECT_STREQ(report.serviceStartName, TEST_SERVICE);
    EXPECT_EQ(report.serviceStartPid, 1);
    EXPECT_EQ(report.serviceStartMode, SERVICES_EXIT_INFO_NOT_SASPAWN);

    service.attribute = SERVICE_ATTR_ONDEMAND;
    TestReportServiceStartByUserInfo(&service, 2);
    TestGetByUserReportSnapshot(&report);
    EXPECT_EQ(report.serviceStartCount, 1);

    TestResetByUserReportSnapshot();
    ReportServiceByUserExit(nullptr, 10, SIGTERM);
    TestGetByUserReportSnapshot(&report);
    EXPECT_EQ(report.childExitCount, 0);

    ServiceByUserId *instance = TestGetOrCreateServiceByUserId(&service, TEST_SERVICE, USER_A);
    ASSERT_NE(instance, nullptr);
    TestSetServiceByUserIdTemplate(TEST_SERVICE, USER_A, nullptr);
    ReportServiceByUserExit(instance, 11, SIGTERM);
    TestGetByUserReportSnapshot(&report);
    EXPECT_EQ(report.childExitCount, 0);

    TestSetServiceByUserIdTemplate(TEST_SERVICE, USER_A, &service);
    ReportServiceByUserExit(instance, 12, SIGTERM);
    TestGetByUserReportSnapshot(&report);
    EXPECT_EQ(report.childExitCount, 1);
    EXPECT_STREQ(report.childExitName, "init_by_user_ut_u401");
    EXPECT_EQ(report.childExitPid, 12);
    EXPECT_EQ(report.childExitCode, SIGTERM);
    EXPECT_EQ(report.childExitMode, SERVICES_EXIT_INFO_NOT_SASPAWN);

    ReportServiceByUserExit(instance, 13, 3 << 8);
    TestGetByUserReportSnapshot(&report);
    EXPECT_EQ(report.childExitCount, 1);
    ResetServicePath(&service);
}

} // namespace init_ut
