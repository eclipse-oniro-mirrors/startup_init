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

#include <cinttypes>
#include <sys/mount.h>
#include "dynamic_service.h"
#include "fs_manager/fs_manager.h"
#include "fs_manager/fs_manager_log.h"
#include "init_log.h"
#include "init_unittest.h"
#include "securec.h"

using namespace testing::ext;
using namespace std;

namespace init_ut {
class InnerkitsUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(InnerkitsUnitTest, TestDynamicService, TestSize.Level1)
{
    const char *name = "updater_sa";
    int32_t ret = StopDynamicProcess(name);
    EXPECT_EQ(ret, 0);
    ret = StartDynamicProcess(name);
    EXPECT_EQ(ret, 0);
    name = "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";
    ret = StopDynamicProcess(name);
    EXPECT_EQ(ret, -1);
    ret = StartDynamicProcess(name);
    EXPECT_EQ(ret, -1);
    name = nullptr;
    ret = StopDynamicProcess(name);
    EXPECT_EQ(ret, -1);
    ret = StartDynamicProcess(name);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(InnerkitsUnitTest, ReadFstabFromFile_unitest, TestSize.Level1)
{
    Fstab *fstab = nullptr;
    const std::string fstabFile1 = "/data/fstab.updater1";
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    EXPECT_EQ(fstab, nullptr);
    const std::string fstabFile2 = "/data/init_ut/mount_unitest/ReadFstabFromFile1.fstable";
    fstab = ReadFstabFromFile(fstabFile2.c_str(), false);
    EXPECT_NE(fstab, nullptr);
    const std::string fstabFile3 = "/data/init_ut/mount_unitest/ReadFstabFromFile2.fstable";
    fstab = ReadFstabFromFile(fstabFile3.c_str(), false);
    EXPECT_NE(fstab, nullptr);
    const std::string fstabFile4 = "/data/init_ut/mount_unitest/ReadFstabFromFile3.fstable";
    fstab = ReadFstabFromFile(fstabFile4.c_str(), false);
    EXPECT_NE(fstab, nullptr);
    const std::string fstabFile5 = "/data/init_ut/mount_unitest/ReadFstabFromFile4.fstable";
    fstab = ReadFstabFromFile(fstabFile5.c_str(), false);
    EXPECT_NE(fstab, nullptr);
    const std::string fstabFile6 = "/data/init_ut/mount_unitest/ReadFstabFromFile5.fstable";
    fstab = ReadFstabFromFile(fstabFile6.c_str(), false);
    EXPECT_NE(fstab, nullptr);
    ReleaseFstab(fstab);
}

HWTEST_F(InnerkitsUnitTest, FindFstabItemForPath_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/init_ut/mount_unitest/FindFstabItemForPath1.fstable";
    Fstab *fstab = nullptr;
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    ASSERT_NE(fstab, nullptr);
    FstabItem* item = nullptr;
    const std::string path1 = "";
    item = FindFstabItemForPath(*fstab, path1.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string path2 = "/data";
    item = FindFstabItemForPath(*fstab, path2.c_str());
    if (item != nullptr) {
        SUCCEED();
    }
    const std::string path3 = "/data2";
    item = FindFstabItemForPath(*fstab, path3.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string path4 = "/data2/test";
    item = FindFstabItemForPath(*fstab, path4.c_str());
    if (item != nullptr) {
        SUCCEED();
    }
    ReleaseFstab(fstab);
    fstab = nullptr;
}

HWTEST_F(InnerkitsUnitTest, FindFstabItemForMountPoint_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/init_ut/mount_unitest/FindFstabItemForMountPoint1.fstable";
    Fstab *fstab = nullptr;
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    ASSERT_NE(fstab, nullptr);
    FstabItem* item = nullptr;
    const std::string mp1 = "/data";
    const std::string mp2 = "/data2";
    item = FindFstabItemForMountPoint(*fstab, mp2.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const std::string mp3 = "/data";
    item = FindFstabItemForMountPoint(*fstab, mp3.c_str());
    if (item != nullptr) {
        SUCCEED();
    }
    ReleaseFstab(fstab);
    fstab = nullptr;
}

HWTEST_F(InnerkitsUnitTest, GetMountFlags_unitest, TestSize.Level1)
{
    const std::string fstabFile1 = "/data/init_ut/mount_unitest/GetMountFlags1.fstable";
    Fstab *fstab = nullptr;
    fstab = ReadFstabFromFile(fstabFile1.c_str(), false);
    ASSERT_NE(fstab, nullptr);
    struct FstabItem* item = nullptr;
    const std::string mp = "/hos";
    item = FindFstabItemForMountPoint(*fstab, mp.c_str());
    if (item == nullptr) {
        SUCCEED();
    }
    const int bufferSize = 512;
    char fsSpecificOptions[bufferSize] = {0};
    unsigned long flags = GetMountFlags(item->mountOptions, fsSpecificOptions, bufferSize);
    EXPECT_EQ(flags, static_cast<unsigned long>(MS_NOSUID | MS_NODEV | MS_NOATIME));
    ReleaseFstab(fstab);
    fstab = nullptr;
}

HWTEST_F(InnerkitsUnitTest, TestFsManagerLog, TestSize.Level1)
{
    FsManagerLogInit(LOG_TO_KERNEL, FILE_NAME);
    FSMGR_LOGE("Fsmanager log to kernel.");
    FsManagerLogInit(LOG_TO_STDIO, "");
    FSMGR_LOGE("Fsmanager log to stdio.");
    string logPath = "/data/init_ut/fs_log.txt";
    auto fp = std::unique_ptr<FILE, decltype(&fclose)>(fopen(logPath.c_str(), "at+"), fclose);
    EXPECT_TRUE(fp != nullptr);
    sync();
    FsManagerLogInit(LOG_TO_FILE, logPath.c_str());
    FSMGR_LOGE("Fsmanager log to file.");
    FsManagerLogDeInit();
}
} // namespace init_ut
