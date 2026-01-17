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
#ifndef APPSPAWN_FS_HVB_IMAGE_PATCH_MOCK_H
#define APPSPAWN_FS_HVB_IMAGE_PATCH_MOCK_H

int MockQuickfixfshvbgetimagecert(struct hvb_cert *cert, char *devName);

int MockQuickfixisimagepatch(const char *partName, int mode);

namespace OHOS {
namespace AppSpawn {
class FsHvbImagePatch {
public:
    virtual ~FsHvbImagePatch() = default;
    virtual int QuickfixFsHvbGetImageCert(struct hvb_cert *cert, char *devName) = 0;

    virtual int QuickfixIsImagePatch(const char *partName, int mode) = 0;

public:
    static inline std::shared_ptr<FsHvbImagePatch> fsHvbImagePatchFunc = nullptr;
};

class FsHvbImagePatchMock : public FsHvbImagePatch {
public:

    MOCK_METHOD(int, QuickfixFsHvbGetImageCert, (struct hvb_cert *cert, char *devName));

    MOCK_METHOD(int, QuickfixIsImagePatch, (const char *partName, int mode));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_FS_HVB_IMAGE_PATCH_MOCK_H
