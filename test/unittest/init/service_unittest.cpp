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

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include "init_cmds.h"
#include "init_service.h"
#include "init_service_manager.h"
#include "init_service_socket.h"
#include "param_stub.h"
#include "init_utils.h"
#include "securec.h"
#include "init_group_manager.h"
#include "trigger_manager.h"
#include "bootstage.h"
#include "init_hook.h"
#include "plugin_adapter.h"

const char *SERVICE_INFO_JSONSTR = "{"
    "\"services\":{"
    "   \"name\":\"test_service\","
    "   \"path\":[\"/data/init_ut/test_service\"],"
    "   \"importance\":-20,"
    "   \"uid\":\"system\","
    "   \"writepid\":[\"/dev/test_service\"],"
    "   \"console\":1,"
    "   \"caps\":[\"TEST_ERR\"],"
    "   \"gid\":[\"system\"],"
    "   \"period\":2,"
    "   \"env\":[{ "
    "      \"name\":\"test\","
    "      \"value\":\"xxxx\""
    "    }],"
    "   \"critical\":1,"
    "   \"jobs\" : {"
    "       \"pre-start\":\"test_service:pre-start\""
    "   }"
    "}"
"}";
const char *JOB_INFO_JSONSTR = "{"
    "\"jobs\": ["
    "   {"
    "       \"name\" : \"test_service:pre-start\","
    "       \"cmds\" : ["
    "           \"setparam test.test.prestartKey 1\""
    "       ]"
    "   }"
    "]"
"}";
const char *PRE_START_KEY = "test.test.prestartKey";
static void DoCmdByName(const char *name, const char *cmdContent)
{
    int cmdIndex = 0;
    (void)GetMatchCmd(name, &cmdIndex);
    DoCmdByIndex(cmdIndex, cmdContent, nullptr);
}

static void LoadJobs()
{
    cJSON *jobJson = cJSON_Parse(JOB_INFO_JSONSTR);
    INIT_ERROR_CHECK(jobJson != NULL, return, "Failed to loadJobs");
    ParseTriggerConfig(jobJson, nullptr, nullptr);
    cJSON_Delete(jobJson);
}

static int ServiceTestTriggerExe(const TriggerNode *trigger, const char *content, uint32_t size)
{
    INIT_ERROR_CHECK(trigger != NULL, return -1, "ParamWriteTriggerExe trigger is NULL");
    CommandNode *cmd = GetNextCmdNode((JobNode *)trigger, NULL);
    while (cmd != NULL) {
        DoCmdByIndex(cmd->cmdKeyIndex, cmd->content, &cmd->cfgContext);
        cmd = GetNextCmdNode((JobNode *)trigger, cmd);
    }
    return 0;
}

using namespace testing::ext;
using namespace std;
namespace init_ut {
class ServiceUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        string svcPath = "/data/init_ut/test_service";
        auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(svcPath.c_str(), "wb"), fclose);
        if (fp == nullptr) {
            cout << "ServiceUnitTest open : " << svcPath << " failed." << errno << endl;
        }
        sync();
    }

    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(ServiceUnitTest, case01, TestSize.Level1)
{
    cJSON* jobItem = cJSON_Parse(SERVICE_INFO_JSONSTR);
    ASSERT_NE(nullptr, jobItem);
    cJSON *serviceItem = cJSON_GetObjectItem(jobItem, "services");
    ASSERT_NE(nullptr, serviceItem);
    Service *service = AddService("test_service");
    int ret = ParseOneService(serviceItem, service);
    EXPECT_EQ(ret, 0);

    LoadJobs();
    RegisterTriggerExec(TRIGGER_UNKNOW, ServiceTestTriggerExe);

    ret = ServiceStart(service, &service->pathArgs);
    EXPECT_EQ(ret, 0);
    
    char value[PARAM_VALUE_LEN_MAX] = {0};
    u_int32_t len = sizeof(value);
    ret = SystemGetParameter(PRE_START_KEY, value, &len);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(0, strcmp("1", value));

    ret = ServiceStop(service);
    EXPECT_EQ(ret, 0);

    ReleaseService(service);
    cJSON_Delete(jobItem);
}

HWTEST_F(ServiceUnitTest, case02, TestSize.Level1)
{
    const char *jsonStr = "{\"services\":{\"name\":\"test_service8\",\"path\":[\"/data/init_ut/test_service\"],"
        "\"importance\":-20,\"uid\":\"system\",\"writepid\":[\"/dev/test_service\"],\"console\":1,"
        "\"gid\":[\"system\", \"shell\", \"root\"],\"caps\":[10, 4294967295, 10000],\"cpucore\":[1]}}";
    cJSON* jobItem = cJSON_Parse(jsonStr);
    ASSERT_NE(nullptr, jobItem);
    cJSON *serviceItem = cJSON_GetObjectItem(jobItem, "services");
    ASSERT_NE(nullptr, serviceItem);
    Service *service = AddService("test_service8");
    ASSERT_NE(nullptr, service);
    int ret = ParseOneService(serviceItem, service);
    EXPECT_EQ(ret, 0);

    int fds[1] = {-1}; // ServiceStop will release fds
    UpdaterServiceFds(service, fds, 1);
    service->attribute = SERVICE_ATTR_ONDEMAND;
    ret = ServiceStart(service, &service->pathArgs);
    EXPECT_EQ(ret, 0);
    CmdLines *cmdline = (CmdLines *)malloc(sizeof(CmdLines) + sizeof(CmdLine));
    ASSERT_NE(nullptr, cmdline);
    cmdline->cmdNum = 1;
    cmdline->cmds[0].cmdIndex = 0;
    service->restartArg = cmdline;
    ServiceSocket tmpSock = { .next = nullptr, .sockFd = 0 };
    ServiceSocket tmpSock1 = { .next = &tmpSock, .sockFd = 0 };
    service->socketCfg = &tmpSock1;
    ServiceReap(service);
    service->socketCfg = nullptr;
    service->attribute &= SERVICE_ATTR_NEED_RESTART;
    service->firstCrashTime = 0;
    ServiceReap(service);
    DoCmdByName("reset ", "test_service8");
    // reset again
    DoCmdByName("reset ", "test_service8");
    service->pid = 0xfffffff; // 0xfffffff is not exist pid
    service->attribute = SERVICE_ATTR_TIMERSTART;
    ret = ServiceStop(service);
    EXPECT_EQ(ret, 0);
    ReleaseService(service);
    cJSON_Delete(jobItem);
}

HWTEST_F(ServiceUnitTest, TestServiceStartAbnormal, TestSize.Level1)
{
    const char *jsonStr = "{\"services\":{\"name\":\"test_service1\",\"path\":[\"/data/init_ut/test_service\"],"
        "\"importance\":-20,\"uid\":\"system\",\"writepid\":[\"/dev/test_service\"],\"console\":1,"
        "\"gid\":[\"system\"],\"caps\":[\"\"]}}";
    cJSON* jobItem = cJSON_Parse(jsonStr);
    ASSERT_NE(nullptr, jobItem);
    cJSON *serviceItem = cJSON_GetObjectItem(jobItem, "services");
    ASSERT_NE(nullptr, serviceItem);
    Service *service = AddService("test_service1");
    ASSERT_NE(nullptr, service);
    int ret = ParseOneService(serviceItem, service);
    EXPECT_EQ(ret, 0);

    ret = ServiceStart(service, &service->pathArgs);
    EXPECT_EQ(ret, 0);

    service->attribute &= SERVICE_ATTR_INVALID;
    ret = ServiceStart(service, &service->pathArgs);
    EXPECT_EQ(ret, 0);

    service->pid = -1;
    ret = ServiceStop(service);
    EXPECT_EQ(ret, 0);
    ReleaseService(service);
    cJSON_Delete(jobItem);
}

HWTEST_F(ServiceUnitTest, TestServiceReap, TestSize.Level1)
{
    Service *service = AddService("test_service2");
    ASSERT_NE(nullptr, service);
    EXPECT_EQ(service->attribute, 0);
    service->attribute = SERVICE_ATTR_ONDEMAND;
    ServiceReap(service);
    service->attribute = 0;

    service->restartArg = (CmdLines *)calloc(1, sizeof(CmdLines));
    ASSERT_NE(nullptr, service->restartArg);
    ServiceReap(service);
    EXPECT_EQ(service->attribute, SERVICE_ATTR_TIMERSTART);

    const int crashCount = 241;
    service->crashCnt = crashCount;
    ServiceReap(service);
    EXPECT_EQ(service->attribute, SERVICE_ATTR_TIMERSTART);

    service->attribute |= SERVICE_ATTR_ONCE;
    ServiceReap(service);
    EXPECT_EQ(service->attribute, (SERVICE_ATTR_ONCE | SERVICE_ATTR_TIMERSTART));
    service->attribute = SERVICE_ATTR_CRITICAL;
    service->crashCount = 1;
    ServiceReap(service);
    ReleaseService(service);
}

HWTEST_F(ServiceUnitTest, TestServiceReapOther, TestSize.Level1)
{
    const char *serviceStr = "{\"services\":{\"name\":\"test_service4\",\"path\":[\"/data/init_ut/test_service\"],"
        "\"onrestart\":[\"sleep 1\"],\"console\":1,\"writepid\":[\"/dev/test_service\"]}}";

    cJSON* jobItem = cJSON_Parse(serviceStr);
    ASSERT_NE(nullptr, jobItem);
    cJSON *serviceItem = cJSON_GetObjectItem(jobItem, "services");
    ASSERT_NE(nullptr, serviceItem);
    Service *service = AddService("test_service4");
    ASSERT_NE(nullptr, service);
    int ret = ParseOneService(serviceItem, service);
    EXPECT_EQ(ret, 0);

    ServiceReap(service);
    EXPECT_NE(service->attribute, 0);

    service->attribute |= SERVICE_ATTR_CRITICAL;
    ServiceReap(service);
    EXPECT_NE(service->attribute, 0);

    service->attribute |= SERVICE_ATTR_NEED_STOP;
    ServiceReap(service);
    EXPECT_NE(service->attribute, 0);

    service->attribute |= SERVICE_ATTR_INVALID;
    ServiceReap(service);
    EXPECT_NE(service->attribute, 0);

    ret = ServiceStop(service);
    EXPECT_EQ(ret, 0);
    ReleaseService(service);
    cJSON_Delete(jobItem);
}

HWTEST_F(ServiceUnitTest, TestServiceManagerRelease, TestSize.Level1)
{
    Service *service = nullptr;
    ReleaseService(service);
    EXPECT_TRUE(service == nullptr);
    service = AddService("test_service5");
    ASSERT_NE(nullptr, service);
    service->pathArgs.argv = (char **)malloc(sizeof(char *));
    ASSERT_NE(nullptr, service->pathArgs.argv);
    service->pathArgs.count = 1;
    const char *path = "/data/init_ut/test_service_release";
    service->pathArgs.argv[0] = strdup(path);

    service->writePidArgs.argv = (char **)malloc(sizeof(char *));
    ASSERT_NE(nullptr, service->writePidArgs.argv);
    service->writePidArgs.count = 1;
    service->writePidArgs.argv[0] = strdup(path);

    service->servPerm.caps = (unsigned int *)malloc(sizeof(unsigned int));
    ASSERT_NE(nullptr, service->servPerm.caps);
    service->servPerm.gIDArray = (gid_t *)malloc(sizeof(gid_t));
    ASSERT_NE(nullptr, service->servPerm.gIDArray);
    service->socketCfg = (ServiceSocket *)malloc(sizeof(ServiceSocket));
    ASSERT_NE(nullptr, service->socketCfg);
    service->socketCfg->sockFd = 0;
    service->socketCfg->next = nullptr;
    service->fileCfg = (ServiceFile *)malloc(sizeof(ServiceFile));
    ASSERT_NE(nullptr, service->fileCfg);
    service->fileCfg->fd = 0;
    service->fileCfg->next = nullptr;
    service->env = (ServiceEnv*)malloc(sizeof(ServiceEnv));
    ASSERT_NE(nullptr, service->env);
    int ret = strcpy_s(service->env->name, MAX_ENV_NAME, "test5");
    EXPECT_EQ(ret, EOK);
    ret = strcpy_s(service->env->value, MAX_ENV_VALUE, "xxxxx");
    EXPECT_EQ(ret, EOK);
    ReleaseService(service);
    service = nullptr;
}

HWTEST_F(ServiceUnitTest, TestServiceManagerGetService, TestSize.Level1)
{
    Service *service = GetServiceByPid(1);
    StopAllServices(1, nullptr, 0, nullptr);
    EXPECT_TRUE(service == nullptr);
    const char *jsonStr = "{\"services\":{\"name\":\"test_service2\",\"path\":[\"/data/init_ut/test_service\"],"
        "\"importance\":-20,\"uid\":\"system\",\"writepid\":[\"/dev/test_service\"],\"console\":1,"
        "\"gid\":[\"system\"], \"critical\":[1,2]}}";
    cJSON* jobItem = cJSON_Parse(jsonStr);
    ASSERT_NE(nullptr, jobItem);
    cJSON *serviceItem = cJSON_GetObjectItem(jobItem, "services");
    ASSERT_NE(nullptr, serviceItem);
    service = AddService("test_service2");
    ASSERT_NE(nullptr, service);
    int ret = ParseOneService(serviceItem, service);
    EXPECT_NE(ret, 0);
    cJSON_Delete(jobItem);
    const char *jsonStr1 = "{\"services\":{\"name\":\"test_service3\",\"path\":[\"/data/init_ut/test_service\"],"
        "\"importance\":-20,\"uid\":\"system\",\"writepid\":[\"/dev/test_service\"],\"console\":1,"
        "\"gid\":[\"system\"], \"critical\":[\"on\"]}}";
    jobItem = cJSON_Parse(jsonStr1);
    ASSERT_NE(nullptr, jobItem);
    serviceItem = cJSON_GetObjectItem(jobItem, "services");
    ASSERT_NE(nullptr, serviceItem);
    service = AddService("test_service3");
    ASSERT_NE(nullptr, service);
    ret = ParseOneService(serviceItem, service);
    EXPECT_NE(ret, 0);
    cJSON_Delete(jobItem);
}

/**
* @tc.name: TestServiceBootEventHook
* @tc.desc: test bootevent module exec correct
* @tc.type: FUNC
* @tc.require: issueI5NTX4
* @tc.author:
*/
HWTEST_F(ServiceUnitTest, TestServiceBootEventHook, TestSize.Level1)
{
    const char *serviceStr = "{"
	    "\"services\": [{"
            "\"name\" : \"test-service\","
            "\"path\" : [\"/dev/test_service\"],"
            "\"start-mode\" : \"condition\","
            "\"writepid\":[\"/dev/test_service\"],"
            "\"bootevents\" : \"bootevent2\""
        "},{"
            "\"name\" : \"test-service\","
            "\"path\" : [\"/dev/test_service\"],"
            "\"start-mode\" : \"condition\","
            "\"writepid\":[\"/dev/test_service\"],"
            "\"bootevents\" : \"bootevent.bootevent2\""
        "},{"
            "\"name\" : \"test-service2\","
            "\"path\" : [\"/dev/test_service\"],"
            "\"console\":1,"
            "\"start-mode\" : \"boot\","
            "\"writepid\":[\"/dev/test_service\"],"
            "\"bootevents\" : [\"bootevent.bootevent1\", \"bootevent.bootevent2\"]"
        "}]"
    "}";

    SERVICE_INFO_CTX serviceInfoContext;
    serviceInfoContext.serviceName = "test-service2";
    serviceInfoContext.reserved = "bootevent";
    HookMgrExecute(GetBootStageHookMgr(), INIT_GLOBAL_INIT, nullptr, nullptr);
    (void)HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_DUMP, (void *)(&serviceInfoContext), nullptr);
    cJSON *fileRoot = cJSON_Parse(serviceStr);
    ASSERT_NE(nullptr, fileRoot);
    ConfigContext context = { INIT_CONTEXT_MAIN };
    ParseAllServices(fileRoot, &context);
    (void)HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_FORK_BEFORE, (void *)(&serviceInfoContext), nullptr);
    serviceInfoContext.reserved = nullptr;
    (void)HookMgrExecute(GetBootStageHookMgr(), INIT_SERVICE_FORK_BEFORE, (void *)(&serviceInfoContext), nullptr);
    const char *initBootevent[] = {"init", "test"};
    PluginExecCmd("bootevent", ARRAY_LENGTH(initBootevent), initBootevent);
    const char *initBooteventDup[] = {"init", "test"};
    PluginExecCmd("bootevent", ARRAY_LENGTH(initBooteventDup), initBooteventDup);
    const char *initBooteventErr[] = {"init"};
    PluginExecCmd("bootevent", ARRAY_LENGTH(initBooteventErr), initBooteventErr);
    SystemWriteParam("bootevent.bootevent1", "true");
    SystemWriteParam("bootevent.bootevent2", "true");
    SystemWriteParam("bootevent.bootevent2", "true");
    SystemWriteParam("persist.init.bootevent.enable", "false");
    HookMgrExecute(GetBootStageHookMgr(), INIT_POST_PERSIST_PARAM_LOAD, nullptr, nullptr);
    cJSON_Delete(fileRoot);
}

HWTEST_F(ServiceUnitTest, TestSetServiceContent, TestSize.Level1)
{
    (void)WatchConsoleDevice(nullptr);
    Service service;
    (void)WatchConsoleDevice(&service);
    int fd = open("/dev/console", O_RDWR);
    if (fd >= 0) {
        (void)write(fd, "0", 1);
        (void)close(fd);
    }
    PluginExecCmdByName("setServiceContent", "netmanager");
    StopServiceByName("netmanager");
}

HWTEST_F(ServiceUnitTest, TestServiceExec, TestSize.Level1)
{
    Service *service = AddService("test_service7");
    ASSERT_NE(nullptr, service);

    service->pathArgs.argv = (char **)malloc(sizeof(char *));
    ASSERT_NE(service->pathArgs.argv, nullptr);
    service->pathArgs.count = 1;
    const char *path = "/data/init_ut/test_service_release";
    service->pathArgs.argv[0] = strdup(path);
    service->importance = 20;
    service->servPerm.gIDCnt = -1;
    service->servPerm.uID = 0;
    unsigned int *caps = (unsigned int *)calloc(1, sizeof(unsigned int) * 1);
    ASSERT_NE(nullptr, caps);
    caps[0] = FULL_CAP;
    service->servPerm.caps = caps;
    service->servPerm.capsCnt = 1;
    IsEnableSandbox();
    EnterServiceSandbox(service);
    int ret = ServiceExec(service, &service->pathArgs);
    EXPECT_EQ(ret, 0);

    const int invalidImportantValue = 20;
    ret = SetImportantValue(service, "", invalidImportantValue, 1);
    EXPECT_EQ(ret, -1);
    ReleaseService(service);
}
} // namespace init_ut
