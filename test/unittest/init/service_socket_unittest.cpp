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
#include <unistd.h>
#include <sys/socket.h>
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
    int ret1 = CreateServiceSocket(service);
    ((WatcherTask *)((ServiceSocket *)service->socketCfg)->watcher)->processEvent(
        LE_GetDefaultLoop(), 0, &eventid, service);
    EXPECT_EQ(ret1, 0);
    ret1 = GetControlSocket(testSocName);
    EXPECT_GE(ret1, 0);
    CloseServiceSocket(service);
    ReleaseService(service);
}
} // namespace init_ut
