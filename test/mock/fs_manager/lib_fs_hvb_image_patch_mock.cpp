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
#include "lib_fs_hvb_image_patch_mock.h"

using namespace OHOS::AppSpawn;
int MockQuickfixfshvbgetimagecert(struct hvb_cert *cert, char *devName)
{
    return FsHvbImagePatch::fsHvbImagePatchFunc->QuickfixFsHvbGetImageCert(struct hvb_cert *cert, char *devName);
}

int MockQuickfixisimagepatch(const char *partName, int mode)
{
    return FsHvbImagePatch::fsHvbImagePatchFunc->QuickfixIsImagePatch(const char *partName, int mode);
}

