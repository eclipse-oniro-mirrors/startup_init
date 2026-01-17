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
#ifndef APPSPAWN_FS_HVB_MOCK_H
#define APPSPAWN_FS_HVB_MOCK_H

int MockFshvbinit(InitHvbType hvbType);

int MockFshvbsetuphashtree(FstabItem *fsItem);

int MockFshvbfinal(InitHvbType hvbType);

int MockFshvbgetvaluefromcmdline(char *val, size_t size, const char *key);

int MockFshvbconstructveritytarget(DmVerityTarget *target, const char *devName, struct hvb_cert *cert);

void MockFshvbdestoryveritytarget(DmVerityTarget *target);

int MockVerifyexthvbimage(const char *devPath, const char *partition, char **outPath);

bool MockCheckandgetext4size(const char *headerBuf, uint64_t *imageSize, const char* image);

bool MockCheckandgeterofssize(const char *headerBuf, uint64_t *imageSize, const char* image);

bool MockCheckandgetextheadersize(const int fd, uint64_t offset, uint64_t *imageSize, const char* image);

namespace OHOS {
namespace AppSpawn {
class FsHvb {
public:
    virtual ~FsHvb() = default;
    virtual int FsHvbInit(InitHvbType hvbType) = 0;

    virtual int FsHvbSetupHashtree(FstabItem *fsItem) = 0;

    virtual int FsHvbFinal(InitHvbType hvbType) = 0;

    virtual int FsHvbGetValueFromCmdLine(char *val, size_t size, const char *key) = 0;

    virtual int FsHvbConstructVerityTarget(DmVerityTarget *target, const char *devName, struct hvb_cert *cert) = 0;

    virtual void FsHvbDestoryVerityTarget(DmVerityTarget *target) = 0;

    virtual int VerifyExtHvbImage(const char *devPath, const char *partition, char **outPath) = 0;

    virtual bool CheckAndGetExt4Size(const char *headerBuf, uint64_t *imageSize, const char* image) = 0;

    virtual bool CheckAndGetErofsSize(const char *headerBuf, uint64_t *imageSize, const char* image) = 0;

    virtual bool CheckAndGetExtheaderSize(const int fd, uint64_t offset, uint64_t *imageSize, const char* image) = 0;

public:
    static inline std::shared_ptr<FsHvb> fsHvbFunc = nullptr;
};

class FsHvbMock : public FsHvb {
public:

    MOCK_METHOD(int, FsHvbInit, (InitHvbType hvbType));

    MOCK_METHOD(int, FsHvbSetupHashtree, (FstabItem *fsItem));

    MOCK_METHOD(int, FsHvbFinal, (InitHvbType hvbType));

    MOCK_METHOD(int, FsHvbGetValueFromCmdLine, (char *val, size_t size, const char *key));

    MOCK_METHOD(int, FsHvbConstructVerityTarget, (DmVerityTarget *target, const char *devName, struct hvb_cert *cert));

    MOCK_METHOD(void, FsHvbDestoryVerityTarget, (DmVerityTarget *target));

    MOCK_METHOD(int, VerifyExtHvbImage, (const char *devPath, const char *partition, char **outPath));

    MOCK_METHOD(bool, CheckAndGetExt4Size, (const char *headerBuf, uint64_t *imageSize, const char* image));

    MOCK_METHOD(bool, CheckAndGetErofsSize, (const char *headerBuf, uint64_t *imageSize, const char* image));

    MOCK_METHOD(bool, CheckAndGetExtheaderSize, (const int fd, uint64_t offset, uint64_t *imageSize, const char* image));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_FS_HVB_MOCK_H
