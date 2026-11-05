/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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


/* White-box access to argument checks; the implementation is unchanged. */
#include "../../../interfaces/innerkits/fs_manager/libfs_hvb/hvb_ops.c"
#include "libfs_hvb_test_adapter.h"

int64_t HvbTestImageSize(int fd, const char *name)
{
    return GetImageSizeForHVB(fd, name);
}
bool HvbTestIsExtPartition(const char *name, size_t length, size_t *index)
{
    return IsExtHvbVerifiedPtn(name, length, index);
}
char *HvbTestExtPath(size_t index)
{
    return GetExtHvbVerifiedPath(index);
}
