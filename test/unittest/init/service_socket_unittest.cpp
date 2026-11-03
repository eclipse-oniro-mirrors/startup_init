/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include "init_service.h"
#include "init_service_manager.h"
#include "init_service_socket.h"
#include "init_socket.h"
#include "param_stub.h"
#include "securec.h"
#include "le_task.h"
using namespace std;
using namespace testing::ext;
namespace init_ut {
class ServiceSocketUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {};
    void TearDown() {};
};
HWTEST_F(ServiceSocketUnitTest, TestCreateSocket, TestSize.Level0)
{
    const char *testSocName = "test_socket";
    uint32_t eventid = 1;
    Service *service = (Service *)AddService("TestCreateSocket");
    ASSERT_NE(service, nullptr);
    service->socketCfg = nullptr;
    service->attribute = SERVICE_ATTR_ONDEMAND;
    ServiceSocket *sockopt = (ServiceSocket *)calloc(1, sizeof(ServiceSocket) + strlen(testSocName) + 1);
    ASSERT_NE(sockopt, nullptr);
    sockopt->type = SOCK_STREAM;
    sockopt->protocol = 0;
    sockopt->family = PF_UNIX;
    sockopt->sockFd = -1;
    sockopt->uid = 1000;
    sockopt->gid = 1000;
    sockopt->perm = 0660;
    sockopt->option = SOCKET_OPTION_PASSCRED;
    errno_t ret = strncpy_s(sockopt->name, strlen(testSocName) + 1, testSocName, strlen(testSocName));
    sockopt->next = nullptr;
    EXPECT_EQ(ret, EOK);
    if (service->socketCfg == nullptr) {
        service->socketCfg = sockopt;
    } else {
        sockopt->next = service->socketCfg->next;
        service->socketCfg->next = sockopt;
    }
    int ret1 = CreateSocketForService(service);
    if (((ServiceSocket *)service->socketCfg)->watcher != nullptr) {
        ((WatcherTask *)((ServiceSocket *)service->socketCfg)->watcher)->processEvent(
            LE_GetDefaultLoop(), 0, &eventid, service);
    }
    EXPECT_EQ(ret1, 0);
    GetControlSocket(nullptr);
    ret1 = GetControlSocket(testSocName);
    EXPECT_GE(ret1, -1);
    CloseServiceSocket(service);
    ReleaseService(service);
}

/* ---- GetControlSocket (init_socket.c) coverage ---- */
static constexpr const char *UT_CTRL_SOCK_NAME = "ut_init_socket_1";
static constexpr const char *UT_CTRL_SOCK_NAME_LONG =
    "ut_init_socket_long_name_abcdefghijklmnopqrstuvwxyz_0123456789_abcdefghijklmnopqrstuvwxyz";

static void UtSetSocketEnv(const char *name, const char *value)
{
    char envName[128] = {0};
    if (snprintf_s(envName, sizeof(envName), sizeof(envName) - 1, "%s%s", OHOS_SOCKET_ENV_PREFIX, name) < 0) {
        return;
    }
    if (value != nullptr) {
        setenv(envName, value, 1);
    } else {
        unsetenv(envName);
    }
}

static int UtCreateUnixSocketAt(const char *dir, const char *name)
{
    mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    char sockPath[256] = {0};
    if (snprintf_s(sockPath, sizeof(sockPath), sizeof(sockPath) - 1, "%s/%s", dir, name) < 0) {
        return -1;
    }
    unlink(sockPath);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    if (strcpy_s(addr.sun_path, sizeof(addr.sun_path), sockPath) != 0) {
        close(fd);
        return -1;
    }
    if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

class InitSocketUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp()
    {
        UtSetSocketEnv(UT_CTRL_SOCK_NAME, nullptr);
    };
    void TearDown() { UtSetSocketEnv(UT_CTRL_SOCK_NAME, nullptr); };
};

/* invalid arguments: null name, env unset, non-numeric value, closed fd, overlong name */
HWTEST_F(InitSocketUnitTest, SocketTest_001, TestSize.Level1)
{
    EXPECT_EQ(GetControlSocket(nullptr), -1);
    UtSetSocketEnv(UT_CTRL_SOCK_NAME, nullptr);
    EXPECT_EQ(GetControlSocket(UT_CTRL_SOCK_NAME), -1);
    UtSetSocketEnv(UT_CTRL_SOCK_NAME, "not-a-number");
    EXPECT_EQ(GetControlSocket(UT_CTRL_SOCK_NAME), -1);
    UtSetSocketEnv(UT_CTRL_SOCK_NAME, "99999");
    EXPECT_EQ(GetControlSocket(UT_CTRL_SOCK_NAME), -1);
    UtSetSocketEnv(UT_CTRL_SOCK_NAME_LONG, "1");
    EXPECT_EQ(GetControlSocket(UT_CTRL_SOCK_NAME_LONG), -1);
}

/* non AF_UNIX socket (inet, not bound): SO_DOMAIN != AF_UNIX, fd is returned directly */
HWTEST_F(InitSocketUnitTest, SocketTest_002, TestSize.Level1)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0);
    char value[16] = {0};
    if (snprintf_s(value, sizeof(value), sizeof(value) - 1, "%d", fd) < 0) {
        close(fd);
        FAIL();
        return;
    }
    UtSetSocketEnv(UT_CTRL_SOCK_NAME, value);
    int ret = GetControlSocket(UT_CTRL_SOCK_NAME);
    EXPECT_EQ(ret, fd);
    close(fd);
}

/* unix socket bound under OHOS_SOCKET_DIR: fd returned (device only, dir must be writable) */
HWTEST_F(InitSocketUnitTest, SocketTest_003, TestSize.Level1)
{
    mkdir(OHOS_SOCKET_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (access(OHOS_SOCKET_DIR, W_OK) != 0) {
        GTEST_SKIP() << OHOS_SOCKET_DIR << " not writable";
    }
    int fd = UtCreateUnixSocketAt(OHOS_SOCKET_DIR, UT_CTRL_SOCK_NAME);
    ASSERT_GE(fd, 0);
    char value[16] = {0};
    if (snprintf_s(value, sizeof(value), sizeof(value) - 1, "%d", fd) < 0) {
        close(fd);
        FAIL();
        return;
    }
    UtSetSocketEnv(UT_CTRL_SOCK_NAME, value);
    int ret = GetControlSocket(UT_CTRL_SOCK_NAME);
    EXPECT_EQ(ret, fd);
    close(fd);
    char sockPath[256] = {0};
    if (snprintf_s(sockPath, sizeof(sockPath), sizeof(sockPath) - 1, "%s/%s", OHOS_SOCKET_DIR, UT_CTRL_SOCK_NAME) > 0) {
        unlink(sockPath);
    }
}

/* unix socket bound to another directory: path prefix mismatch, -1 returned */
HWTEST_F(InitSocketUnitTest, SocketTest_004, TestSize.Level1)
{
    const char *otherDir = STARTUP_INIT_UT_PATH "/socket";
    int fd = UtCreateUnixSocketAt(otherDir, UT_CTRL_SOCK_NAME);
    ASSERT_GE(fd, 0);
    char value[16] = {0};
    if (snprintf_s(value, sizeof(value), sizeof(value) - 1, "%d", fd) < 0) {
        close(fd);
        FAIL();
        return;
    }
    UtSetSocketEnv(UT_CTRL_SOCK_NAME, value);
    EXPECT_EQ(GetControlSocket(UT_CTRL_SOCK_NAME), -1);
    close(fd);
    char sockPath[256] = {0};
    if (snprintf_s(sockPath, sizeof(sockPath), sizeof(sockPath) - 1, "%s/%s", otherDir, UT_CTRL_SOCK_NAME) > 0) {
        unlink(sockPath);
    }
}
} // namespace init_ut
