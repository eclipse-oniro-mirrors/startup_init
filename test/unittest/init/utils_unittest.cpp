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
#include "param_stub.h"
#include "init_utils.h"
#include "init_service_socket.h"
#include "init_socket.h"
#include "securec.h"
using namespace std;
using namespace testing::ext;

extern "C" {
float ConvertMicrosecondToSecond(int x);
}
#define MAX_BUFFER_FOR_TEST 128

namespace init_ut {
class UtilsUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(UtilsUnitTest, TestMakeDir, TestSize.Level0)
{
    const char *dir = "/data/init_ut/mkdir_test";
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH | S_IROTH;
    int ret = MakeDirRecursive(dir, mode);
    EXPECT_EQ(ret, 0);
    dir = nullptr;
    ret = MakeDirRecursive(dir, mode);
    EXPECT_EQ(ret, -1);
    ret = MakeDir(dir, 9999);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(UtilsUnitTest, TestString, TestSize.Level0)
{
    const char *str = nullptr;
    int defaultValue = 1;
    int ret = StringToInt(str, defaultValue);
    EXPECT_EQ(ret, 1);
    str = "10";
    ret = StringToInt(str, defaultValue);
    EXPECT_EQ(ret, 10);
    char rStr[] = "abc";
    char oldChr = 'a';
    char newChr = 'd';
    ret = StringReplaceChr(rStr, oldChr, newChr);
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(rStr, "dbc");
}
HWTEST_F(UtilsUnitTest, TestUtilsApi, TestSize.Level0)
{
    float sec = ConvertMicrosecondToSecond(1000000); // 1000000 microseconds
    EXPECT_EQ(sec, 1);
    EXPECT_EQ(WriteAll(2, "test", strlen("test")), 4);
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    CheckAndCreatFile("/data/init_ut/testcreatfile", mode);
    CheckAndCreatFile("/data/init_ut/nodir/testcreatfile", mode);
    char testStr[] = ".trim";
    EXPECT_STREQ(TrimHead(testStr, '.'), "trim");
}

void TestOpenConsole(void)
{
    // Save current stdin, stdout, stderr
    int fd0 = dup(STDIN_FILENO);
    EXPECT_GT(fd0, 0);
    int fd1 = dup(STDOUT_FILENO);
    EXPECT_GT(fd1, 0);
    int fd2 = dup(STDERR_FILENO);
    EXPECT_GT(fd2, 0);
    // test start
    OpenConsole();
    char buffer[MAX_BUFFER_FOR_TEST];
    ssize_t ret = readlink("/proc/self/fd/0", buffer, MAX_BUFFER_FOR_TEST - 1);
    EXPECT_NE(ret, -1);
    auto n = strcmp(buffer, "/dev/console");
    EXPECT_EQ(n, 0);
    (void)memset_s(buffer, sizeof(buffer), 0, MAX_BUFFER_FOR_TEST);
    ret = readlink("/proc/self/fd/1", buffer, MAX_BUFFER_FOR_TEST - 1);
    EXPECT_NE(ret, -1);
    n = strcmp(buffer, "/dev/console");
    EXPECT_EQ(n, 0);
    (void)memset_s(buffer, sizeof(buffer), 0, MAX_BUFFER_FOR_TEST);
    ret = readlink("/proc/self/fd/2", buffer, MAX_BUFFER_FOR_TEST - 1);
    EXPECT_NE(ret, -1);
    n = strcmp(buffer, "/dev/console");
    EXPECT_EQ(n, 0);
    // test done
    // restore stdin, stdout, stderr
    dup2(fd0, 0);
    dup2(fd1, 1);
    dup2(fd2, 2); // 2 stderr
    close(fd0);
    close(fd1);
    close(fd2);
}

HWTEST_F(UtilsUnitTest, TestUtilsOpenConsole, TestSize.Level0)
{
    TestOpenConsole();
}
} // namespace init_ut
