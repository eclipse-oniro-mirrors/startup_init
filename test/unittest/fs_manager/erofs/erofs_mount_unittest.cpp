/*
* Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "erofs_mount_overlay.h"
#include "param_stub.h"
#include "init_utils.h"

using namespace std;
using namespace testing::ext;

namespace init_ut {
class ErofsMountUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp(void) {};
    void TearDown(void) {};
};

HWTEST_F(ErofsMountUnitTest, Init_AllocDmName_001, TestSize.Level0)
{
    char nameExt4[MAX_BUFFER_LEN] = {0};
    char nameRofs[MAX_BUFFER_LEN] = {0};
    const char *devName = STARTUP_INIT_UT_PATH"/data/erofs/mount/rofs";
    AllocDmName(devName, nameRofs, MAX_BUFFER_LEN, nameExt4, MAX_BUFFER_LEN);
    EXPECT_STREQ("_data_init_ut_data_erofs_mount_rofs_erofs", nameRofs);
    EXPECT_STREQ("_data_init_ut_data_erofs_mount_rofs_ext4", nameExt4);
}

HWTEST_F(ErofsMountUnitTest, Init_LookupErofsEnd_001, TestSize.Level0)
{
    const char *devMount = STARTUP_INIT_UT_PATH"/data/erofs/mount/lookup";
    int ret = LookupErofsEnd(devMount);
    EXPECT_EQ(ret, 0);
    mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    CheckAndCreatFile(devMount, mode);
    int fd = open(devMount, O_WRONLY | O_TRUNC);
    void *data = calloc(1, EROFS_SUPER_BLOCK_START_POSITION);
    ret = write(fd, data, EROFS_SUPER_BLOCK_START_POSITION);
    EXPECT_EQ(ret, EROFS_SUPER_BLOCK_START_POSITION);

    close(fd);
    free(data);
    ret = LookupErofsEnd(devMount);
    EXPECT_EQ(ret, 0);
    struct erofs_super_block sb;
    sb.magic = EROFS_SUPER_MAGIC;
    sb.blocks = 1;
    data = calloc(1, sizeof(sb));
    memcpy_s(data, EROFS_SUPER_BLOCK_START_POSITION, &sb, sizeof(sb));
    fd = open(devMount, O_WRONLY | O_APPEND);
    ret = write(fd, data, sizeof(sb));
    EXPECT_EQ(ret, sizeof(sb));

    close(fd);
    free(data);
    ret = LookupErofsEnd(devMount);
    EXPECT_NE(ret, 0);
    remove(devMount);
}
}