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
#include <cstring> // for strncmp

// 函数实现
static bool UnderBasicMountPoint(const char *path)
{
    unsigned int i;
    if (path == nullptr || *path == '\0') {
        return false;
    }
    const char *basicMountPoint[] = {"/dev/", "/sys/", "/proc/"};
    for (i = 0; i < sizeof(basicMountPoint) / sizeof(basicMountPoint[0]); i++) {
        if (strncmp(path, basicMountPoint[i], strlen(basicMountPoint[i])) == 0) {
            return true;
        }
    }
    return false;
}

// 测试类
class UnderBasicMountPointTest : public ::testing::Test {
protected:
    // 可以在此处进行测试前的初始化
    void SetUp() override {}

    // 可以在此处进行测试后的清理
    void TearDown() override {}
};

// 测试NULL路径
TEST_F(UnderBasicMountPointTest, TestNullPath)
{
    EXPECT_FALSE(UnderBasicMountPoint(nullptr));
}

// 测试空路径
TEST_F(UnderBasicMountPointTest, TestEmptyPath)
{
    EXPECT_FALSE(UnderBasicMountPoint(""));
}

// 测试路径以 "/dev/" 开头
TEST_F(UnderBasicMountPointTest, TestDevPath)
{
    EXPECT_TRUE(UnderBasicMountPoint("/dev/sda"));
}

// 测试路径以 "/sys/" 开头
TEST_F(UnderBasicMountPointTest, TestSysPath)
{
    EXPECT_TRUE(UnderBasicMountPoint("/sys/class"));
}

// 测试路径以 "/proc/" 开头
TEST_F(UnderBasicMountPointTest, TestProcPath)
{
    EXPECT_TRUE(UnderBasicMountPoint("/proc/cpuinfo"));
}

// 测试路径不以任何基本挂载点开头
TEST_F(UnderBasicMountPointTest, TestNonBasicPath)
{
    EXPECT_FALSE(UnderBasicMountPoint("/home/user/file.txt"));
}

// 测试路径仅包含一个基本挂载点前缀
TEST_F(UnderBasicMountPointTest, TestBasicMountPointExactMatch)
{
    EXPECT_TRUE(UnderBasicMountPoint("/dev"));
    EXPECT_TRUE(UnderBasicMountPoint("/sys"));
    EXPECT_TRUE(UnderBasicMountPoint("/proc"));
}

// 测试路径以基本挂载点前缀加其他字符（包含类似 "/dev/random"）
TEST_F(UnderBasicMountPointTest, TestBasicMountPointWithSuffix)
{
    EXPECT_TRUE(UnderBasicMountPoint("/dev/random"));
    EXPECT_TRUE(UnderBasicMountPoint("/sys/module"));
    EXPECT_TRUE(UnderBasicMountPoint("/proc/uptime"));
}

// 测试路径以大小写不匹配的字符串
TEST_F(UnderBasicMountPointTest, TestCaseSensitiveMatch)
{
    EXPECT_FALSE(UnderBasicMountPoint("/DEV/random"));
    EXPECT_FALSE(UnderBasicMountPoint("/SYS/module"));
    EXPECT_FALSE(UnderBasicMountPoint("/PROC/uptime"));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
