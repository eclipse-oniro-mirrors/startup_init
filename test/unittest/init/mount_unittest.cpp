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
#include <fcntl.h>
#include "fs_manager/fs_manager.h"
#include "param_stub.h"
#include "init_mount.h"
#include "init.h"
#include "securec.h"
using namespace std;
using namespace testing::ext;

namespace init_ut {
class MountUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(MountUnitTest, TestMountRequriedPartitions, TestSize.Level0)
{
    const char *fstabFiles = "/data/init_ut/etc/fstab.required";
    Fstab *fstab = NULL;
    fstab = ReadFstabFromFile(fstabFiles, false);
    if (fstab != NULL) {
#ifdef __MUSL__
        int ret = MountRequriedPartitions(fstab);
        EXPECT_EQ(ret, -1);
#endif
        ReleaseFstab(fstab);
    } else {
        Fstab fstab1;
        fstab1.head = nullptr;
        int ret = MountRequriedPartitions(&fstab1);
        EXPECT_EQ(ret, -1);
    }
    ReleaseFstab(LoadRequiredFstab());
}
HWTEST_F(MountUnitTest, TestGetBlockDevicePath, TestSize.Level1)
{
    char path[20] = {0}; // 20 is path length
    int fd = open("/bin/updater", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,  S_IRWXU);
    if (fd < 0) {
        return;
    }
    GetBlockDevicePath("/test", path, sizeof(path));
    close(fd);
    ReadConfig();
    unlink("/bin/updater");
    ReadConfig();
    int ret = GetBlockDeviceByMountPoint(nullptr, nullptr, nullptr, 0);
    EXPECT_EQ(ret, -1);
    FstabItem fstabitem = {(char *)"deviceName", (char *)"mountPoint",
        (char *)"fsType", (char *)"mountOptions", 1, nullptr};
    Fstab fstab = {&fstabitem};
    char devicename[20]; // 20 is devicename length
    ret = GetBlockDeviceByMountPoint("notmountpoint", &fstab, devicename, sizeof(devicename));
    EXPECT_EQ(ret, -1);
    ret = GetBlockDeviceByMountPoint("mountPoint", &fstab, devicename, 0);
    EXPECT_EQ(ret, -1);
    ret = GetBlockDeviceByMountPoint("mountPoint", &fstab, devicename, sizeof(devicename));
    EXPECT_EQ(ret, 0);
}
} // namespace init_ut
