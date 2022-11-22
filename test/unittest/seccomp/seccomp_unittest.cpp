/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <asm/unistd.h>
#include <syscall.h>
#include <climits>

#include "seccomp_policy.h"

using SyscallFunc = bool (*)(void);
constexpr int SLEEP_TIME = 100000; // 100ms

using namespace testing::ext;
using namespace std;

namespace init_ut {
class SeccompUnitTest : public testing::Test {
public:
    SeccompUnitTest() {};
    virtual ~SeccompUnitTest() {};
    static void SetUpTestCase() {};
    static void TearDownTestCase() {};
    void SetUp() {};
    void TearDown() {};
    void TestBody(void) {};
    static void Handler(int s)
    {
    }

    static pid_t StartChild(const char *filterName, SyscallFunc func)
    {
        pid_t pid = fork();
        if (pid == 0) {
            if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
                std::cout << "PR_SET_NO_NEW_PRIVS set fail " << std::endl;
                exit(EXIT_FAILURE);
            }
            if (!SetSeccompPolicyWithName(filterName)) {
                std::cout << "SetSeccompPolicy set fail fiterName is " << filterName << std::endl;
                exit(EXIT_FAILURE);
            }

            if (!func()) {
                std::cout << "func excute fail" << std::endl;
                exit(EXIT_FAILURE);
            }

            std::cout << "func excute success" << std::endl;

            exit(EXIT_SUCCESS);
        }
        return pid;
    }

    static int CheckSyscall(const char *filterName, SyscallFunc func, bool isAllow)
    {
        sigset_t set;
        int status;
        pid_t pid;
        int flag = 0;
        struct timespec waitTime = {5, 0};

        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);
        sigprocmask(SIG_BLOCK, &set, nullptr);
        if (signal(SIGCHLD, Handler) == nullptr) {
            std::cout << "signal failed:" << strerror(errno) << std::endl;
        }

        /* Sleeping for avoiding influencing child proccess wait for other threads
         * which were created by other unittests to release global rwlock. The global
         * rwlock will be used by function dlopen in child process */
        usleep(SLEEP_TIME);

        pid = StartChild(filterName, func);
        if (pid == -1) {
            std::cout << "fork failed:" << strerror(errno) << std::endl;
            return -1;
        }
        if (sigtimedwait(&set, nullptr, &waitTime) == -1) { /* Wait for 5 seconds */
            if (errno == EAGAIN) {
                flag = 1;
            } else {
                std::cout << "sigtimedwait failed:" << strerror(errno) << std::endl;
            }

            if (kill(pid, SIGKILL) == -1) {
                std::cout << "kill failed::" << strerror(errno) << std::endl;
            }
        }

        if (waitpid(pid, &status, 0) != pid) {
            std::cout << "waitpid failed:" << strerror(errno) << std::endl;
            return -1;
        }

        if (flag != 0) {
            std::cout << "Child process time out" << std::endl;
        }

        if (WEXITSTATUS(status) == EXIT_FAILURE) {
            return -1;
        }

        if (WIFSIGNALED(status)) {
            if (WTERMSIG(status) == SIGSYS) {
                    std::cout << "child process exit with SIGSYS" << std::endl;
                    return isAllow ? -1 : 0;
            }
        } else {
            std::cout << "child process finished normally" << std::endl;
            return isAllow ? 0 : -1;
        }
        return -1;
    }

#if defined __aarch64__
    static bool CheckGetMempolicy()
    {
        int ret = syscall(__NR_get_mempolicy, NULL, NULL, 0, NULL, 0);
        if (ret < 0) {
            return false;
        }

        return true;
    }

    static bool CheckGetpid()
    {
        pid_t pid = 1;
        pid = syscall(__NR_getpid);
        if (pid > 1) {
            return true;
        }
        return false;
    }

    static bool CheckGetuid()
    {
        uid_t uid = 0;
        uid = syscall(__NR_getuid);
        if (uid >= 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuidArgsInRange()
    {
        int ret = syscall(__NR_setresuid, 20000, 20000, 20000);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuidArgsOutOfRange()
    {
        int ret = syscall(__NR_setresuid, 800, 800, 800);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetuid()
    {
        int uid = syscall(__NR_setuid, 1);
        if (uid == 0) {
            return true;
        }

        return false;
    }

    void TestSystemSycall()
    {
        // system blocklist
        int ret = CheckSyscall(SYSTEM_NAME, CheckGetMempolicy, false);
        EXPECT_EQ(ret, 0);

        // system allowlist
        ret = CheckSyscall(SYSTEM_NAME, CheckGetpid, true);
        EXPECT_EQ(ret, 0);
    }

    void TestSetUidGidFilter()
    {
        // system blocklist
        int ret = CheckSyscall(APPSPAWN_NAME, CheckSetresuidArgsOutOfRange, false);
        EXPECT_EQ(ret, 0);

        // system allowlist
        ret = CheckSyscall(APPSPAWN_NAME, CheckSetresuidArgsInRange, true);
        EXPECT_EQ(ret, 0);
    }

    void TestAppSycall()
    {
        // app blocklist
        int ret = CheckSyscall(APP_NAME, CheckSetuid, false);
        EXPECT_EQ(ret, 0);

        // app allowlist
        ret = CheckSyscall(APP_NAME, CheckGetpid, true);
        EXPECT_EQ(ret, 0);
    }
#elif defined __arm__
    static bool CheckGetuid32()
    {
        uid_t uid = syscall(__NR_getuid32);
        if (uid >= 0) {
            return true;
        }
        return false;
    }

    static bool CheckGetuid()
    {
        uid_t uid = syscall(__NR_getuid);
        if (uid >= 0) {
            return true;
        }
        return false;
    }

    static bool CheckSetuid32()
    {
        int ret = syscall(__NR_setuid32, 1);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ArgsInRange()
    {
        int ret = syscall(__NR_setresuid32, 20000, 20000, 20000);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ArgsOutOfRange()
    {
        int ret = syscall(__NR_setresuid32, 800, 800, 800);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    void TestSystemSycall()
    {
        // system blocklist
        int ret = CheckSyscall(SYSTEM_NAME, CheckGetuid, false);
        EXPECT_EQ(ret, 0);

        // system allowlist
        ret = CheckSyscall(SYSTEM_NAME, CheckGetuid32, true);
        EXPECT_EQ(ret, 0);
    }

    void TestSetUidGidFilter()
    {
        // system blocklist
        int ret = CheckSyscall(APPSPAWN_NAME, CheckSetresuid32ArgsOutOfRange, false);
        EXPECT_EQ(ret, 0);

        // system allowlist
        ret = CheckSyscall(APPSPAWN_NAME, CheckSetresuid32ArgsInRange, true);
        EXPECT_EQ(ret, 0);
    }

    void TestAppSycall()
    {
        // app blocklist
        int ret = CheckSyscall(APP_NAME, CheckSetuid32, false);
        EXPECT_EQ(ret, 0);

        // app allowlist
        ret = CheckSyscall(APP_NAME, CheckGetuid32, true);
        EXPECT_EQ(ret, 0);
    }
#endif
};

/**
 * @tc.name: TestSystemSycall
 * @tc.desc: Verify the system seccomp policy.
 * @tc.type: FUNC
 * @tc.require: issueI5IUWJ
 */
HWTEST_F(SeccompUnitTest, TestSystemSycall, TestSize.Level1)
{
    SeccompUnitTest test;
    test.TestSystemSycall();
}

/**
 * @tc.name: TestSetUidGidFilter
 * @tc.desc: Verify the system seccomp policy.
 * @tc.type: FUNC
 * @tc.require: issueI5IUWJ
 */
HWTEST_F(SeccompUnitTest, TestSetUidGidFilter, TestSize.Level1)
{
    SeccompUnitTest test;
    test.TestSetUidGidFilter();
}

/**
 * @tc.name: TestSystemSycall
 * @tc.desc: Verify the system seccomp policy.
 * @tc.type: FUNC
 * @tc.require: issueI5MUXD
 */
HWTEST_F(SeccompUnitTest, TestAppSycall, TestSize.Level1)
{
    SeccompUnitTest test;
    test.TestAppSycall();
}
}
