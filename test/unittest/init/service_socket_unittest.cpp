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
#include "init_unittest.h"
#include "securec.h"
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
    Service *service = (Service *)calloc(1, sizeof(Service));
    ASSERT_NE(service, nullptr);
    ServiceSocket *sockopt = (ServiceSocket *)calloc(1, sizeof(ServiceSocket));
    ASSERT_NE(sockopt, nullptr);
    sockopt->type = SOCK_SEQPACKET;
    sockopt->sockFd = -1;
    sockopt->uid = 1000;
    sockopt->gid = 1000;
    sockopt->perm = 0660;
    sockopt->passcred = true;
    const char *testSocName = "test_socket";
    errno_t ret = strncpy_s(sockopt->name, strlen(testSocName) + 1, testSocName, strlen(testSocName));
    sockopt->next = NULL;
    EXPECT_EQ(ret, EOK);
    if (service->socketCfg == NULL) {
        service->socketCfg = sockopt;
    } else {
        sockopt->next = service->socketCfg->next;
        service->socketCfg->next = sockopt;
    }
    int ret1 = CreateServiceSocket(service);
    EXPECT_EQ(ret1, 0);
    ret1 = GetControlSocket(testSocName);
    EXPECT_GE(ret1, 0);
    CloseServiceSocket(service);
    ReleaseService(service);
}
} // namespace init_ut
