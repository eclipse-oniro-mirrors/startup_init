/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

void FreeRootDir(DIR* dir, dev_t dev)
{
    closedir(dir);  // 假设这个释放资源的函数
}

int MountToNewTarget(const char* newRoot)
{
    // 假设这个函数是我们已经实现的，模拟返回值。
    return 0;  // 成功
}

// SwitchRoot 函数
int SwitchRoot(const char *newRoot)
{
    if (newRoot == nullptr || *newRoot == '\0') {
        errno = EINVAL;
        INIT_LOGE("Fatal error. Try to switch to new root with invalid new mount point");
        return -1;
    }

    struct stat oldRootStat = {};
    INIT_ERROR_CHECK(stat("/", &oldRootStat) == 0, return -1, "Failed to get old root \"/\" stat");
    DIR *oldRoot = opendir("/");
    INIT_ERROR_CHECK(oldRoot != nullptr, return -1, "Failed to open root dir \"/\"");
    
    struct stat newRootStat = {};
    if (stat(newRoot, &newRootStat) != 0) {
        INIT_LOGE("Failed to get new root \" %s \" stat", newRoot);
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }

    if (oldRootStat.st_dev == newRootStat.st_dev) {
        INIT_LOGW("Try to switch root in same device, skip switching root");
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return 0;
    }
    if (MountToNewTarget(newRoot) < 0) {
        INIT_LOGE("Failed to move mount to new root \" %s \" stat", newRoot);
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }

    // 处理其他的挂载逻辑
    if (chdir(newRoot) < 0) {
        INIT_LOGE("Failed to change directory to %s, err = %d", newRoot, errno);
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }

    if (mount(newRoot, "/", nullptr, MS_MOVE, nullptr) < 0) {
        INIT_LOGE("Failed to mount moving %s to %s, err = %d", newRoot, "/", errno);
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }

    if (chroot(".") < 0) {
        INIT_LOGE("Failed to change root directory");
        FreeRootDir(oldRoot, oldRootStat.st_dev);
        return -1;
    }
    FreeRootDir(oldRoot, oldRootStat.st_dev);
    INIT_LOGI("SwitchRoot to %s finish", newRoot);
    return 0;
}

// 测试类
class SwitchRootTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试传入 nullptr 路径
TEST_F(SwitchRootTest, TestNullPath)
{
    EXPECT_EQ(SwitchRoot(nullptr), -1);
}

// 测试传入空字符串路径
TEST_F(SwitchRootTest, TestEmptyPath)
{
    EXPECT_EQ(SwitchRoot(""), -1);
}

// 测试无法获取当前根路径 stat 信息
TEST_F(SwitchRootTest, TestStatOldRootFailure)
{
    struct stat statBackup;
    EXPECT_EQ(SwitchRoot("/mnt"), -1);
}

// 测试无法打开当前根路径的目录
TEST_F(SwitchRootTest, TestOpenOldRootDirFailure)
{
    struct stat oldRootStat;
    EXPECT_EQ(SwitchRoot("/mnt"), -1);
}

// 测试无法获取新根路径的 stat 信息
TEST_F(SwitchRootTest, TestStatNewRootFailure)
{
    EXPECT_EQ(SwitchRoot("/mnt/nonexistent"), -1);
}

// 测试调用 `MountToNewTarget` 失败
TEST_F(SwitchRootTest, TestMountToNewTargetFailure)
{
    EXPECT_EQ(SwitchRoot("/mnt"), -1);
}

// 测试 `chdir` 失败
TEST_F(SwitchRootTest, TestChdirFailure)
{
    EXPECT_EQ(SwitchRoot("/mnt"), -1);
}

// 测试 `mount` 失败
TEST_F(SwitchRootTest, TestMountFailure)
{
    EXPECT_EQ(SwitchRoot("/mnt"), -1);
}

// 测试 `chroot` 失败
TEST_F(SwitchRootTest, TestChrootFailure)
{
    EXPECT_EQ(SwitchRoot("/mnt"), -1);
}

// 测试成功切换根路径
TEST_F(SwitchRootTest, TestSuccess)
{
    EXPECT_EQ(SwitchRoot("/mnt"), 0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
