/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include <cstdlib>
#include "init_cmds.h"
#include "init_group_manager.h"
#include "init_hashmap.h"
#include "init_param.h"
#include "init_service_manager.h"
#include "init_utils.h"
#include "init_unittest.h"
#include "le_timer.h"
#include "param_stub.h"
#include "securec.h"
#include "control_fd.h"

using namespace testing::ext;
using namespace std;

typedef struct {
    HashNode node;
    char name[0];
} TestHashNode;

static int TestHashNodeCompare(const HashNode *node1, const HashNode *node2)
{
    TestHashNode *testNode1 = HASHMAP_ENTRY(node1, TestHashNode, node);
    TestHashNode *testNode2 = HASHMAP_ENTRY(node2, TestHashNode, node);
    return strcmp(testNode1->name, testNode2->name);
}

static int TestHashKeyCompare(const HashNode *node1, const void *key)
{
    TestHashNode *testNode1 = HASHMAP_ENTRY(node1, TestHashNode, node);
    return strcmp(testNode1->name, reinterpret_cast<char *>(const_cast<void *>(key)));
}

static int TestHashNodeFunction(const HashNode *node)
{
    TestHashNode *testNode = HASHMAP_ENTRY(node, TestHashNode, node);
    int code = 0;
    size_t nameLen = strlen(testNode->name);
    for (size_t i = 0; i < nameLen; i++) {
        code += testNode->name[i] - 'A';
    }
    return code;
}

static int TestHashKeyFunction(const void *key)
{
    int code = 0;
    char *buff = const_cast<char *>(static_cast<const char *>(key));
    size_t buffLen = strlen(buff);
    for (size_t i = 0; i < buffLen; i++) {
        code += buff[i] - 'A';
    }
    return code;
}

static void TestHashNodeFree(const HashNode *node)
{
    TestHashNode *testNode = HASHMAP_ENTRY(node, TestHashNode, node);
    printf("TestHashNodeFree %s\n", testNode->name);
    free(testNode);
}

static TestHashNode *TestCreateHashNode(const char *value)
{
    TestHashNode *node = reinterpret_cast<TestHashNode *>(malloc(sizeof(TestHashNode) + strlen(value) + 1));
    if (node == nullptr) {
        return nullptr;
    }
    int ret = strcpy_s(node->name, strlen(value) + 1, value);
    if (ret != 0) {
        free(node);
        return nullptr;
    }
    HASHMAPInitNode(&node->node);
    return node;
}

namespace init_ut {
class InitGroupManagerUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HashInfo g_info = {
    TestHashNodeCompare,
    TestHashKeyCompare,
    TestHashNodeFunction,
    TestHashKeyFunction,
    TestHashNodeFree,
    2
};

HWTEST_F(InitGroupManagerUnitTest, TestHashMap, TestSize.Level1)
{
    HashMapHandle handle;
    OH_HashMapCreate(&handle, &g_info);
    const char *str1 = "Test hash map node 1";
    const char *str2 = "Test hash map node 2";
    const char *str3 = "Test hash map node 3";
    TestHashNode *node1 = TestCreateHashNode(str1);
    TestHashNode *node2 = TestCreateHashNode(str2);
    OH_HashMapAdd(handle, &node1->node);
    OH_HashMapAdd(handle, &node2->node);
    HashNode *node = OH_HashMapGet(handle, (const void *)str1);
    EXPECT_NE(node != nullptr, 0);
    if (node) {
        TestHashNode *tmp = HASHMAP_ENTRY(node, TestHashNode, node);
        EXPECT_EQ(strcmp(tmp->name, str1), 0);
    }
    node = OH_HashMapGet(handle, (const void *)str2);
    EXPECT_NE(node != nullptr, 0);
    if (node) {
        TestHashNode *tmp = HASHMAP_ENTRY(node, TestHashNode, node);
        EXPECT_EQ(strcmp(tmp->name, str2), 0);
    }
    TestHashNode *node3 = TestCreateHashNode(str3);
    OH_HashMapAdd(handle, &node3->node);
    node3 = TestCreateHashNode("Test hash map node 4");
    OH_HashMapAdd(handle, &node3->node);
    node3 = TestCreateHashNode("Test hash map node 5");
    OH_HashMapAdd(handle, &node3->node);
    node = OH_HashMapGet(handle, (const void *)str3);
    EXPECT_NE(node != nullptr, 0);
    if (node) {
        TestHashNode *tmp = HASHMAP_ENTRY(node, TestHashNode, node);
        EXPECT_EQ(strcmp(tmp->name, str3), 0);
    }
    TestHashNode *node4 = TestCreateHashNode("pre-init");
    OH_HashMapAdd(handle, &node4->node);

    const char *act = "load_persist_props_action";
    TestHashNode *node5 = TestCreateHashNode(act);
    OH_HashMapAdd(handle, &node5->node);
    OH_HashMapRemove(handle, "pre-init");
    node = OH_HashMapGet(handle, (const void *)act);
    EXPECT_NE(node != nullptr, 0);
    if (node) {
        TestHashNode *tmp = HASHMAP_ENTRY(node, TestHashNode, node);
        EXPECT_EQ(strcmp(tmp->name, act), 0);
    }
    OH_HashMapIsEmpty(handle);
    OH_HashMapTraverse(handle, [](const HashNode *node, const void *context) {return;}, nullptr);
    OH_HashMapDestory(handle);
}

HWTEST_F(InitGroupManagerUnitTest, TestInitGroupMgrInit, TestSize.Level1)
{
    InitServiceSpace();
    InitWorkspace *workspace = GetInitWorkspace();
    EXPECT_EQ(workspace->groupMode, GROUP_CHARGE);
    workspace->groupMode = GROUP_BOOT;
    if (strcpy_s(workspace->groupModeStr, GROUP_NAME_MAX_LENGTH, "device.boot.group") != EOK) {
        EXPECT_EQ(1, 0);
    } // test read cfgfile
    int ret = InitParseGroupCfg();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(InitGroupManagerUnitTest, TestAddService, TestSize.Level1)
{
    const char *serviceStr = "{"
	    "\"services\": [{"
            "\"name\" : \"test-service\","
            "\"path\" : [\"/dev/test_service\"],"
            "\"start-mode\" : \"condition\","
            "\"end-mode\" : \"after-exec\","
            "\"console\":1,"
            "\"writepid\":[\"/dev/test_service\"],"
            "\"jobs\" : {"
                    "\"on-boot\" : \"boot:bootjob1\","
                    "\"on-start\" : \"service:startjob\","
                    "\"on-stop\" : \"service:stopjob\","
                    "\"on-restart\" : \"service:restartjob\""
                "}"
        "},{"
            "\"name\" : \"test-service2\","
            "\"path\" : [\"/dev/test_service\"],"
            "\"console\":1,"
            "\"start-mode\" : \"boot\","
            "\"writepid\":[\"/dev/test_service\"],"
            "\"jobs\" : {"
                    "\"on-boot\" : \"boot:bootjob1\","
                    "\"on-start\" : \"service:startjob\","
                    "\"on-stop\" : \"service:stopjob\","
                    "\"on-restart\" : \"service:restartjob\""
                "}"
        "}]"
    "}";

    cJSON *fileRoot = cJSON_Parse(serviceStr);
    ASSERT_NE(nullptr, fileRoot);
    ParseAllServices(fileRoot);
    cJSON_Delete(fileRoot);

    Service *service = GetServiceByName("test-service");
    ServiceStartTimer(service, 1);
    ((TimerTask *)service->timer)->base.handleEvent(LE_GetDefaultLoop(), (LoopBase *)service->timer, Event_Read);
    ServiceStopTimer(service);
    ASSERT_NE(service != nullptr, 0);
    EXPECT_EQ(service->startMode, START_MODE_CONDITION);
    ReleaseService(service);
    service = GetServiceByName("test-service2");
    ASSERT_NE(service != nullptr, 0);
    EXPECT_EQ(service->startMode, START_MODE_BOOT);
    ReleaseService(service);
}

/**
 * @brief
 *

    "socket" : [{
        "name" : "ueventd",
        "family" : "AF_INET", // AF_INET,AF_INET6,AF_UNIX(AF_LOCAL),AF_NETLINK
        "type" : : "SOCK_STREAM", // SOCK_STREAM,SOCK_DGRAM,SOCK_RAW,SOCK_PACKET,SOCK_SEQPACKET
        "protocol" : "IPPROTO_TCP", // IPPROTO_TCP,IPPTOTO_UDP,IPPROTO_SCTP,PPROTO_TIPC
        "permissions" : "0660",
        "uid" : "system",
        "gid" : "system",
        "option" : {
            "passcred" : "true",
            "rcvbufforce" : "",
            "cloexec" : "",
            "nonblock : ""
        }
    }],
 */
HWTEST_F(InitGroupManagerUnitTest, TestAddServiceDeny, TestSize.Level1)
{
    const char *serviceStr = "{"
	    "\"services\": [{"
            "\"name\" : \"test-service5\","
            "\"path\" : [\"/dev/test_service\"],"
            "\"start-mode\" : \"by-condition\","
            "\"end-mode\" : \"ready\","
            "\"console\":1,"
            "\"writepid\":[\"/dev/test_service\"],"
            "\"jobs\" : {"
                    "\"on-boot\" : \"boot:bootjob1\","
                    "\"on-start\" : \"service:startjob\","
                    "\"on-stop\" : \"service:stopjob\","
                    "\"on-restart\" : \"service:restartjob\""
                "}"
        "}]"
    "}";
    InitWorkspace *workspace = GetInitWorkspace();
    workspace->groupMode = GROUP_CHARGE;

    cJSON *fileRoot = cJSON_Parse(serviceStr);
    ASSERT_NE(nullptr, fileRoot);
    ParseAllServices(fileRoot);
    cJSON_Delete(fileRoot);

    Service *service = GetServiceByName("test-service5");
    ASSERT_EQ(service, nullptr);
    workspace->groupMode = GROUP_BOOT;
}

HWTEST_F(InitGroupManagerUnitTest, TestAddService2, TestSize.Level1)
{
    const char *serviceStr = "{"
	    "\"services\": [{"
            "\"name\" : \"test-service6\","
            "\"path\" : [\"/dev/test_service\"],"
            "\"console\":1,"
            "\"writepid\":[\"/dev/test_service\"],"
            "\"jobs\" : {"
                    "\"on-boot\" : \"boot:bootjob1\","
                    "\"on-start\" : \"service:startjob\","
                    "\"on-stop\" : \"service:stopjob\","
                    "\"on-restart\" : \"service:restartjob\""
                "}"
        "}]"
    "}";

    InitWorkspace *workspace = GetInitWorkspace();
    workspace->groupMode = GROUP_CHARGE;

    InitGroupNode *node = AddGroupNode(NODE_TYPE_SERVICES, "test-service6");
    ASSERT_NE(nullptr, node);

    cJSON *fileRoot = cJSON_Parse(serviceStr);
    ASSERT_NE(nullptr, fileRoot);
    ParseAllServices(fileRoot);
    cJSON_Delete(fileRoot);
    char cmdStr[] = "all#bootevent";
    char cmdStr1[] = "parameter_service";
    ProcessControlFd(ACTION_DUMP, "all", NULL);
    ProcessControlFd(ACTION_DUMP, cmdStr, NULL);
    ProcessControlFd(ACTION_DUMP, cmdStr1, NULL);
    ProcessControlFd(ACTION_SANDBOX, cmdStr, NULL);
    ProcessControlFd(ACTION_MODULEMGR, cmdStr, NULL);
    ProcessControlFd(ACTION_MAX, cmdStr, NULL);
    Service *service = GetServiceByName("test-service6");
    ASSERT_NE(service, nullptr);
    workspace->groupMode = GROUP_BOOT;
}

HWTEST_F(InitGroupManagerUnitTest, TestParseServiceCpucore, TestSize.Level1)
{
    const char *jsonStr = "{\"services\":{\"name\":\"test_service22\",\"path\":[\"/data/init_ut/test_service\"],"
        "\"importance\":-20,\"uid\":\"root\",\"writepid\":[\"/dev/test_service\"],\"console\":1,"
        "\"gid\":[\"root\"], \"cpucore\":[5, 2, 4, 1, 2, 0, 1], \"critical\":[1]}}";
    cJSON* jobItem = cJSON_Parse(jsonStr);
    ASSERT_NE(nullptr, jobItem);
    cJSON *serviceItem = cJSON_GetObjectItem(jobItem, "services");
    ASSERT_NE(nullptr, serviceItem);
    Service *service = AddService("test_service22");
    const int invalidImportantValue = 20;
    SetImportantValue(service, "", invalidImportantValue, 1);
    if (service != nullptr) {
        int ret = ParseOneService(serviceItem, service);
        GetAccessToken();
        DoCmdByName("timer_start ", "test_service22|5000");
        DoCmdByName("timer_start ", "test_service22|aa");
        DoCmdByName("timer_start ", "");
        EXPECT_EQ(ret, 0);
        StartServiceByName("test_service22|path");
        ReleaseService(service);
    }
    cJSON_Delete(jobItem);
}
HWTEST_F(InitGroupManagerUnitTest, TestNodeFree, TestSize.Level1)
{
    DoCmdByName("stopAllServices ", "");
}
HWTEST_F(InitGroupManagerUnitTest, TestUpdaterServiceFds, TestSize.Level1)
{
    Service *service = AddService("test_service8");
    ASSERT_NE(nullptr, service);
    int *fds = (int *)malloc(sizeof(int) * 1); // ServiceStop will release fds
    UpdaterServiceFds(nullptr, nullptr, 0);
    UpdaterServiceFds(service, fds, 1);
    UpdaterServiceFds(service, fds, 0);
    UpdaterServiceFds(service, fds, 1);
    UpdaterServiceFds(service, nullptr, 1);
    UpdaterServiceFds(service, fds, 1);
    int ret = UpdaterServiceFds(service, nullptr, 2); // 2 is fd num
    ASSERT_NE(ret, 0);
    service->attribute = SERVICE_ATTR_TIMERSTART;
    ServiceStartTimer(service, 0);
}
HWTEST_F(InitGroupManagerUnitTest, TestProcessWatchEvent, TestSize.Level1)
{
    Service *service = AddService("test_service9");
    ASSERT_NE(nullptr, service);
    ServiceSocket servercfg = {.next = nullptr, .sockFd = 0};
    service->socketCfg = &servercfg;
    ServiceWatcher watcher;
    int ret = SocketAddWatcher(&watcher, service, 0);
    ASSERT_EQ(ret, 0);
    uint32_t event;
    ((WatcherTask *)watcher)->processEvent((WatcherHandle)watcher, 0, &event, service);
}
}  // namespace init_ut
