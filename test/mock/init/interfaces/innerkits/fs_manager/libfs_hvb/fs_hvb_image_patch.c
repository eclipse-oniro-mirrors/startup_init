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

#include "fs_hvb_image_patch.h"
#include "beget_ext.h"

#define MOCK_PATCH_NAME "patch_test"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

int QuickfixFsHvbGetImageCert(struct hvb_cert *cert, char *devName)
{
    BEGET_LOGW("virtual get image cert");
    return -1;
}

int QuickfixIsImagePatch(const char *partName, int mode)
{
    BEGET_LOGW("virtual is image patch mock");
    if (partName == NULL) {
        return -1;
    }
    if (strcmp(partName, MOCK_PATCH_NAME) == 0) {
        return 0;
    } else {
        return -1;
    }
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
