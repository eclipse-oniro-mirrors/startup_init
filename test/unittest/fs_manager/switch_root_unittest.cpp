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

#include <gtest/gtest.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "func_wrapper.h"
#include "param_stub.h"
extern "C" {
int SwitchRoot(const char *newRoot);
}

using namespace testing::ext;
using namespace std;

namespace init_ut {
#define UT_DIR STARTUP_INIT_UT_PATH "/switch_root"
#define UT_FILE UT_DIR "/ut_new_root_file"
#define UT_FAKE_ROOT UT_DIR "/fake_root"

static int UtBlockUnlinkat(int dirfd, const char *pathname, int flags)
{
    (void)dirfd;
    (void)pathname;
    (void)flags;
    return 0;
}

// opendir wrap owned by init_unittest only; asm label keeps the ld --wrap mandated link name
DIR *WrapOpendir(const char *name) __asm__("__wrap_opendir");
DIR *WrapOpendirReal(const char *name) __asm__("__real_opendir");
static bool g_utRedirectOldRoot = false;

DIR *WrapOpendir(const char *name)
{
    if (g_utRedirectOldRoot && strcmp(name, "/") == 0) {
        return WrapOpendirReal(UT_FAKE_ROOT);
    }
    return WrapOpendirReal(name);
}

class SwitchRootUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        mkdir(UT_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        FILE *fp = fopen(UT_FILE, "w");
        if (fp != nullptr) {
            (void)fclose(fp);
        }
        mkdir(UT_FAKE_ROOT, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        mkdir(UT_FAKE_ROOT "/nested", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        fp = fopen(UT_FAKE_ROOT "/f1", "w");
        if (fp != nullptr) {
            (void)fclose(fp);
        }
        fp = fopen(UT_FAKE_ROOT "/nested/f2", "w");
        if (fp != nullptr) {
            (void)fclose(fp);
        }
    };
    static void TearDownTestCase(void)
    {
        unlink(UT_FAKE_ROOT "/nested/f2");
        unlink(UT_FAKE_ROOT "/f1");
        rmdir(UT_FAKE_ROOT "/nested");
        rmdir(UT_FAKE_ROOT);
        unlink(UT_FILE);
        rmdir(UT_DIR);
    };
    void SetUp()
    {
        UpdateUnlinkatFunc(UtBlockUnlinkat);
        g_utRedirectOldRoot = true;
    };
    void TearDown()
    {
        UpdateUnlinkatFunc(nullptr);
        g_utRedirectOldRoot = false;
        SetStubResult(STUB_MOUNT, 0);
    };
};

/* null / empty new root is rejected with EINVAL, nonexistent new root fails stat */
HWTEST_F(SwitchRootUnitTest, SwitchRootTest_001, TestSize.Level1)
{
    errno = 0;
    EXPECT_EQ(SwitchRoot(nullptr), -1);
    EXPECT_EQ(errno, EINVAL);
    errno = 0;
    EXPECT_EQ(SwitchRoot(""), -1);
    EXPECT_EQ(errno, EINVAL);
    EXPECT_EQ(SwitchRoot("/no/such/dir/for/ut"), -1);
}

/* new root on the same device as "/": switch is skipped, 0 returned, entries under fake old root survive */
HWTEST_F(SwitchRootUnitTest, SwitchRootTest_002, TestSize.Level1)
{
    struct stat rootStat = {};
    struct stat newStat = {};
    ASSERT_EQ(stat("/", &rootStat), 0);
    ASSERT_EQ(stat("/", &newStat), 0);
    if (rootStat.st_dev != newStat.st_dev) {
        return;
    }
    EXPECT_EQ(SwitchRoot("/"), 0);
    EXPECT_EQ(access(UT_FAKE_ROOT "/f1", F_OK), 0);
    EXPECT_EQ(access(UT_FAKE_ROOT "/nested/f2", F_OK), 0);
}

/* moving mounts fails (mount stubbed to -1): switching root fails */
HWTEST_F(SwitchRootUnitTest, SwitchRootTest_003, TestSize.Level1)
{
    struct stat rootStat = {};
    struct stat newStat = {};
    ASSERT_EQ(stat("/", &rootStat), 0);
    ASSERT_EQ(stat(UT_DIR, &newStat), 0);
    if (rootStat.st_dev == newStat.st_dev) {
        return;
    }
    SetStubResult(STUB_MOUNT, -1);
    EXPECT_EQ(SwitchRoot(UT_DIR), -1);
    chdir("/");
}

/* new root is a regular file: chdir fails after moving mounts */
HWTEST_F(SwitchRootUnitTest, SwitchRootTest_004, TestSize.Level1)
{
    struct stat rootStat = {};
    struct stat newStat = {};
    ASSERT_EQ(stat("/", &rootStat), 0);
    ASSERT_EQ(stat(UT_FILE, &newStat), 0);
    if (rootStat.st_dev == newStat.st_dev) {
        return;
    }
    EXPECT_EQ(SwitchRoot(UT_FILE), -1);
}
} // namespace init_ut
