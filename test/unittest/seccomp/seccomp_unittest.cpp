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
#include <sched.h>

#include "seccomp_policy.h"

using SyscallFunc = bool (*)(void);
constexpr int SLEEP_TIME_100MS = 100000; // 100ms
constexpr int SLEEP_TIME_1S = 1;

using namespace testing::ext;
using namespace std;

namespace init_ut {
class SeccompUnitTest : public testing::Test {
public:
    SeccompUnitTest() {};
    virtual ~SeccompUnitTest() {};
    static void SetUpTestCase() {};
    static void TearDownTestCase() {};

    void SetUp()
    {
        /*
         * Wait for 1 second to prevent the generated crash file
         * from being overwritten because the crash interval is too short
         * and the crash file's name is constructed by time stamp.
         */
        sleep(SLEEP_TIME_1S);
    };

    void TearDown() {};
    void TestBody(void) {};

    static pid_t StartChild(SeccompFilterType type, const char *filterName, SyscallFunc func)
    {
        pid_t pid = fork();
        if (pid == 0) {
            if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
                std::cout << "PR_SET_NO_NEW_PRIVS set fail " << std::endl;
                exit(EXIT_FAILURE);
            }

            if (!SetSeccompPolicyWithName(type, filterName)) {
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

    static int CheckStatus(int status, bool isAllow)
    {
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

    static int CheckSyscall(SeccompFilterType type, const char *filterName, SyscallFunc func, bool isAllow)
    {
        sigset_t set;
        int status;
        pid_t pid;
        int flag = 0;
        struct timespec waitTime = {5, 0};

        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);
        sigprocmask(SIG_BLOCK, &set, nullptr);
        sigaddset(&set, SIGSYS);
        if (signal(SIGCHLD, SIG_DFL) == nullptr) {
            std::cout << "signal failed:" << strerror(errno) << std::endl;
        }
        if (signal(SIGSYS, SIG_DFL) == nullptr) {
            std::cout << "signal failed:" << strerror(errno) << std::endl;
        }

        /* Sleeping for avoiding influencing child proccess wait for other threads
         * which were created by other unittests to release global rwlock. The global
         * rwlock will be used by function dlopen in child process */
        usleep(SLEEP_TIME_100MS);

        pid = StartChild(type, filterName, func);
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

        return CheckStatus(status, isAllow);
    }

    static bool CheckUnshare()
    {
        int ret = unshare(CLONE_NEWPID);
        if (ret) {
            return false;
        }
        return true;
    }

    static bool CheckSetns()
    {
        int fd = open("/proc/1/ns/mnt", O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return false;
        }

        if (setns(fd, CLONE_NEWNS) != 0) {
            return false;
        }

        close(fd);
        return true;
    }

    static int ChildFunc(void *arg)
    {
        exit(0);
    }

    static bool CheckCloneNs(int flag)
    {
        const int stackSize = 65536;

        char *stack = static_cast<char *>(malloc(stackSize));
        if (stack == nullptr) {
            return false;
        }
        char *stackTop = stack + stackSize;
        pid_t pid = clone(ChildFunc, stackTop, flag | SIGCHLD, nullptr);
        if (pid == -1) {
            return false;
        }
        return true;
    }

    static bool CheckClonePidNs(void)
    {
        return CheckCloneNs(CLONE_NEWPID);
    }

    static bool CheckCloneMntNs(void)
    {
        return CheckCloneNs(CLONE_NEWNS);
    }

    static bool CheckCloneNetNs(void)
    {
        return CheckCloneNs(CLONE_NEWNET);
    }

    static bool CheckCloneCgroupNs(void)
    {
        return CheckCloneNs(CLONE_NEWCGROUP);
    }

    static bool CheckCloneUtsNs(void)
    {
        return CheckCloneNs(CLONE_NEWUTS);
    }

    static bool CheckCloneIpcNs(void)
    {
        return CheckCloneNs(CLONE_NEWIPC);
    }

    static bool CheckCloneUserNs(void)
    {
        return CheckCloneNs(CLONE_NEWUSER);
    }

#if defined __aarch64__
    static bool CheckMqOpen()
    {
        int ret = (int)syscall(__NR_mq_open, nullptr, 0);
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

    static bool CheckSetuid64ForUidFilter1()
    {
        int ret = syscall(__NR_setuid, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetuid64ForUidFilter2()
    {
        int ret = syscall(__NR_setuid, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid64ForUidFilter1()
    {
        int ret = syscall(__NR_setreuid, 0, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid64ForUidFilter2()
    {
        int ret = syscall(__NR_setreuid, 2, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid64ForUidFilter3()
    {
        int ret = syscall(__NR_setreuid, 0, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid64ForUidFilter4()
    {
        int ret = syscall(__NR_setreuid, 2, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetfsuid64ForUidFilter1()
    {
        int ret = syscall(__NR_setfsuid, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetfsuid64ForUidFilter2()
    {
        int ret = syscall(__NR_setfsuid, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid64ForUidFilter1()
    {
        int ret = syscall(__NR_setresuid, 0, 0, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid64ForUidFilter2()
    {
        int ret = syscall(__NR_setresuid, 2, 0, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid64ForUidFilter3()
    {
        int ret = syscall(__NR_setresuid, 0, 2, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid64ForUidFilter4()
    {
        int ret = syscall(__NR_setresuid, 0, 0, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid64ForUidFilter5()
    {
        int ret = syscall(__NR_setresuid, 0, 2, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid64ForUidFilter6()
    {
        int ret = syscall(__NR_setresuid, 2, 0, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid64ForUidFilter7()
    {
        int ret = syscall(__NR_setresuid, 2, 2, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid64ForUidFilter8()
    {
        int ret = syscall(__NR_setresuid, 2, 2, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    void TestSystemSycall()
    {
        // system blocklist
        int ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckMqOpen, false);
        EXPECT_EQ(ret, 0);

        // system allowlist
        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckGetpid, true);
        EXPECT_EQ(ret, 0);
    }

    void TestSystemSyscallForUidFilter()
    {
        // system_uid_filter_64bit_test
        int ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetuid64ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetuid64ForUidFilter2, true);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid64ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid64ForUidFilter2, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid64ForUidFilter3, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid64ForUidFilter4, true);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetfsuid64ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetfsuid64ForUidFilter2, true);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid64ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid64ForUidFilter2, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid64ForUidFilter3, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid64ForUidFilter4, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid64ForUidFilter5, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid64ForUidFilter6, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid64ForUidFilter7, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid64ForUidFilter8, true);
        EXPECT_EQ(ret, 0);
    }

    void TestSetUidGidFilter()
    {
        // system blocklist
        int ret = CheckSyscall(INDIVIDUAL, APPSPAWN_NAME, CheckSetresuidArgsOutOfRange, false);
        EXPECT_EQ(ret, 0);

        // system allowlist
        ret = CheckSyscall(INDIVIDUAL, APPSPAWN_NAME, CheckSetresuidArgsInRange, true);
        EXPECT_EQ(ret, 0);
    }

    void TestAppSycall()
    {
        // app blocklist
        int ret = CheckSyscall(APP, APP_NAME, CheckSetuid, false);
        EXPECT_EQ(ret, 0);

        // app allowlist
        ret = CheckSyscall(APP, APP_NAME, CheckGetpid, true);
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

    static bool CheckSetuid32ForUidFilter1()
    {
        int ret = syscall(__NR_setuid32, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetuid32ForUidFilter2()
    {
        int ret = syscall(__NR_setuid32, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetuid16ForUidFilter1()
    {
        int ret = syscall(__NR_setuid, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetuid16ForUidFilter2()
    {
        int ret = syscall(__NR_setuid, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid32ForUidFilter1()
    {
        int ret = syscall(__NR_setreuid32, 0, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid32ForUidFilter2()
    {
        int ret = syscall(__NR_setreuid32, 2, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid32ForUidFilter3()
    {
        int ret = syscall(__NR_setreuid32, 0, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid32ForUidFilter4()
    {
        int ret = syscall(__NR_setreuid32, 2, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid16ForUidFilter1()
    {
        int ret = syscall(__NR_setreuid, 0, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid16ForUidFilter2()
    {
        int ret = syscall(__NR_setreuid, 2, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid16ForUidFilter3()
    {
        int ret = syscall(__NR_setreuid, 0, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetreuid16ForUidFilter4()
    {
        int ret = syscall(__NR_setreuid, 2, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetfsuid32ForUidFilter1()
    {
        int ret = syscall(__NR_setfsuid32, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetfsuid32ForUidFilter2()
    {
        int ret = syscall(__NR_setfsuid32, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetfsuid16ForUidFilter1()
    {
        int ret = syscall(__NR_setfsuid, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetfsuid16ForUidFilter2()
    {
        int ret = syscall(__NR_setfsuid, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ForUidFilter1()
    {
        int ret = syscall(__NR_setresuid32, 0, 0, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ForUidFilter2()
    {
        int ret = syscall(__NR_setresuid32, 2, 0, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ForUidFilter3()
    {
        int ret = syscall(__NR_setresuid32, 0, 2, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ForUidFilter4()
    {
        int ret = syscall(__NR_setresuid32, 0, 0, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ForUidFilter5()
    {
        int ret = syscall(__NR_setresuid32, 0, 2, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ForUidFilter6()
    {
        int ret = syscall(__NR_setresuid32, 2, 0, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ForUidFilter7()
    {
        int ret = syscall(__NR_setresuid32, 2, 2, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid32ForUidFilter8()
    {
        int ret = syscall(__NR_setresuid32, 2, 2, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid16ForUidFilter1()
    {
        int ret = syscall(__NR_setresuid, 0, 0, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid16ForUidFilter2()
    {
        int ret = syscall(__NR_setresuid, 2, 0, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid16ForUidFilter3()
    {
        int ret = syscall(__NR_setresuid, 0, 2, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid16ForUidFilter4()
    {
        int ret = syscall(__NR_setresuid, 0, 0, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid16ForUidFilter5()
    {
        int ret = syscall(__NR_setresuid, 0, 2, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid16ForUidFilter6()
    {
        int ret = syscall(__NR_setresuid, 2, 0, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid16ForUidFilter7()
    {
        int ret = syscall(__NR_setresuid, 2, 2, 0);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    static bool CheckSetresuid16ForUidFilter8()
    {
        int ret = syscall(__NR_setresuid, 2, 2, 2);
        if (ret == 0) {
            return true;
        }

        return false;
    }

    void TestSystemSycall()
    {
        // system blocklist
        int ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckGetuid, false);
        EXPECT_EQ(ret, 0);

        // system allowlist
        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckGetuid32, true);
        EXPECT_EQ(ret, 0);
    }

    void TestSystemSyscallForUidFilter32Bit()
    {
        // system_uid_filter_32bit_test
        int ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetuid32ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetuid32ForUidFilter2, true);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid32ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid32ForUidFilter2, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid32ForUidFilter3, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid32ForUidFilter4, true);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetfsuid32ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetfsuid32ForUidFilter2, true);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid32ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid32ForUidFilter2, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid32ForUidFilter3, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid32ForUidFilter4, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid32ForUidFilter5, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid32ForUidFilter6, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid32ForUidFilter7, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid32ForUidFilter8, true);
        EXPECT_EQ(ret, 0);
    }

    void TestSystemSyscallForUidFilter16Bit()
    {
        // system_uid_filter_16bit_test
        int ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetuid16ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetuid16ForUidFilter2, true);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid16ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid16ForUidFilter2, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid16ForUidFilter3, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetreuid16ForUidFilter4, true);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetfsuid16ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetfsuid16ForUidFilter2, true);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid16ForUidFilter1, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid16ForUidFilter2, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid16ForUidFilter3, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid16ForUidFilter4, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid16ForUidFilter5, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid16ForUidFilter6, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid16ForUidFilter7, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(SYSTEM_SA, SYSTEM_NAME, CheckSetresuid16ForUidFilter8, true);
        EXPECT_EQ(ret, 0);
    }

    void TestSystemSyscallForUidFilter()
    {
        TestSystemSyscallForUidFilter32Bit();
        TestSystemSyscallForUidFilter16Bit();
    }

    void TestSetUidGidFilter()
    {
        // system blocklist
        int ret = CheckSyscall(INDIVIDUAL, APPSPAWN_NAME, CheckSetresuid32ArgsOutOfRange, false);
        EXPECT_EQ(ret, 0);

        // system allowlist
        ret = CheckSyscall(INDIVIDUAL, APPSPAWN_NAME, CheckSetresuid32ArgsInRange, true);
        EXPECT_EQ(ret, 0);
    }

    void TestAppSycall()
    {
        // app blocklist
        int ret = CheckSyscall(APP, APP_NAME, CheckSetuid32, false);
        EXPECT_EQ(ret, 0);

        // app allowlist
        ret = CheckSyscall(APP, APP_NAME, CheckGetuid32, true);
        EXPECT_EQ(ret, 0);
    }
#endif
    void TestAppSycallNs()
    {
        int ret = CheckSyscall(APP, APP_NAME, CheckUnshare, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(APP, APP_NAME, CheckSetns, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(APP, APP_NAME, CheckClonePidNs, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(APP, APP_NAME, CheckCloneMntNs, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(APP, APP_NAME, CheckCloneCgroupNs, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(APP, APP_NAME, CheckCloneIpcNs, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(APP, APP_NAME, CheckCloneUserNs, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(APP, APP_NAME, CheckCloneNetNs, false);
        EXPECT_EQ(ret, 0);

        ret = CheckSyscall(APP, APP_NAME, CheckCloneUtsNs, false);
        EXPECT_EQ(ret, 0);
    }
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
 * @tc.desc: Verify the uid gid seccomp policy.
 * @tc.type: FUNC
 * @tc.require: issueI5IUWJ
 */
HWTEST_F(SeccompUnitTest, TestSetUidGidFilter, TestSize.Level1)
{
    SeccompUnitTest test;
    test.TestSetUidGidFilter();
}

/**
 * @tc.name: TestAppSycall
 * @tc.desc: Verify the app seccomp policy.
 * @tc.type: FUNC
 * @tc.require: issueI5MUXD
 */
HWTEST_F(SeccompUnitTest, TestAppSycall, TestSize.Level1)
{
    SeccompUnitTest test;
    test.TestAppSycall();
}

/**
 * @tc.name: TestSystemSyscallForUidFilter
 * @tc.desc: Verify the system seccomp policy.
 * @tc.type: FUNC
 * @tc.require: issueI7QET2
 */
HWTEST_F(SeccompUnitTest, TestSystemSyscallForUidFilter, TestSize.Level1)
{
    SeccompUnitTest test;
    test.TestSystemSyscallForUidFilter();
}

/**
 * @tc.name: TestAppSycallNs
 * @tc.desc: Verify the app seccomp policy about namespace.
 * @tc.type: FUNC
 * @tc.require: issueI8LZTC
 */
HWTEST_F(SeccompUnitTest, TestAppSycallNs, TestSize.Level1)
{
    SeccompUnitTest test;
    test.TestAppSycallNs();
}
}
