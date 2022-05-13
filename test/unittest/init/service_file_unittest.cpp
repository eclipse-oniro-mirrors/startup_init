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
#include "init_service_file.h"
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
    fileOpt->next = NULL;
    fileOpt->flags = O_RDWR;
    fileOpt->uid = 1000;
    fileOpt->gid = 1000;
    fileOpt->fd = -1;
    fileOpt->perm = 0770;
    if (strncpy_s(fileOpt->fileName, strlen(fileName) + 1, fileName, strlen(fileName)) != 0) {
        free(fileOpt);
        fileOpt = nullptr;
        FAIL();
    }
    CreateServiceFile(fileOpt);
    int ret = GetControlFile("/data/filetest");
    EXPECT_NE(ret, -1);
    if (strncpy_s(fileOpt->fileName, strlen(fileName) + 1, "fileName", strlen("fileName")) != 0) {
        free(fileOpt);
        fileOpt = nullptr;
        FAIL();
    }
    fileOpt->fd = 100; // 100 is fd
    CreateServiceFile(fileOpt);
    CloseServiceFile(fileOpt);
    free(fileOpt);
    fileOpt = nullptr;
}
}
