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

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/statvfs.h>

#include "init_file.h"
#include "init_service.h"
#include "init_service_file.h"
#include "init_service_manager.h"
#include "param_stub.h"
#include "securec.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class ServiceFileUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(ServiceFileUnitTest, TestServiceFile, TestSize.Level1)
{
    const char *fileName = "/data/filetest";
    ServiceFile *fileOpt = (ServiceFile *)calloc(1, sizeof(ServiceFile) + strlen(fileName) + 1);
    ASSERT_NE(fileOpt, nullptr);
    fileOpt->next = nullptr;
    fileOpt->flags = O_RDWR;
    fileOpt->uid = 1000;
    fileOpt->gid = 1000;
    fileOpt->fd = -1;
    fileOpt->perm = 0770;
    int ret = strncpy_s(fileOpt->fileName, strlen(fileName) + 1, fileName, strlen(fileName));
    EXPECT_EQ(ret, 0);
    Service *service = AddService("test_service8");
    ASSERT_NE(nullptr, service);

    service->fileCfg = fileOpt;
    CreateServiceFile(service);
    ret = GetControlFile("/data/filetest");
    EXPECT_NE(ret, -1);
    ret = strncpy_s(fileOpt->fileName, strlen(fileName) + 1, "fileName", strlen("fileName"));
    EXPECT_EQ(ret, 0);
    fileOpt->fd = 100; // 100 is fd
    CreateServiceFile(service);
    CloseServiceFile(fileOpt);
    ret = strncpy_s(fileOpt->fileName, strlen(fileName) + 1, "/dev/filetest", strlen("/dev/filetest"));
    EXPECT_EQ(ret, 0);
    char *wrongName = (char *)malloc(PATH_MAX);
    ASSERT_NE(wrongName, nullptr);
    EXPECT_EQ(memset_s(wrongName, PATH_MAX, 1, PATH_MAX), 0);
    ret = GetControlFile(wrongName);
    EXPECT_NE(ret, 0);
    GetControlFile("notExist");
    GetControlFile(nullptr);
    EXPECT_EQ(setenv("OHOS_FILE_ENV_PREFIX_testPath", "5", 0), 0);
    GetControlFile("testPath");
    EXPECT_EQ(setenv("OHOS_FILE_ENV_PREFIX_testPath1", "aaaaaaaa", 0), 0);
    GetControlFile("testPath1");
    free(wrongName);

    fileOpt->fd = -1;
    CreateServiceFile(service);
    CloseServiceFile(fileOpt);
    free(fileOpt);
    service->fileCfg = nullptr;
    ReleaseService(service);
}
}
