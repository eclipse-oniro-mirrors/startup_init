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
#include "lib_fs_hvb_mock.h"

using namespace OHOS::AppSpawn;
int MockFshvbinit(InitHvbType hvbType)
{
    return FsHvb::fsHvbFunc->FsHvbInit(InitHvbType hvbType);
}

int MockFshvbsetuphashtree(FstabItem *fsItem)
{
    return FsHvb::fsHvbFunc->FsHvbSetupHashtree(FstabItem *fsItem);
}

int MockFshvbfinal(InitHvbType hvbType)
{
    return FsHvb::fsHvbFunc->FsHvbFinal(InitHvbType hvbType);
}

int MockFshvbgetvaluefromcmdline(char *val, size_t size, const char *key)
{
    return FsHvb::fsHvbFunc->FsHvbGetValueFromCmdLine(char *val, size_t size, const char *key);
}

int MockFshvbconstructveritytarget(DmVerityTarget *target, const char *devName, struct hvb_cert *cert)
{
    return FsHvb::fsHvbFunc->FsHvbConstructVerityTarget(DmVerityTarget *target, const char *devName, struct hvb_cert *cert);
}

void MockFshvbdestoryveritytarget(DmVerityTarget *target)
{
    FsHvb::fsHvbFunc->FsHvbDestoryVerityTarget(DmVerityTarget *target);
}

int MockVerifyexthvbimage(const char *devPath, const char *partition, char **outPath)
{
    return FsHvb::fsHvbFunc->VerifyExtHvbImage(const char *devPath, const char *partition, char **outPath);
}

bool MockCheckandgetext4size(const char *headerBuf, uint64_t *imageSize, const char* image)
{
    return FsHvb::fsHvbFunc->CheckAndGetExt4Size(const char *headerBuf, uint64_t *imageSize, const char* image);
}

bool MockCheckandgeterofssize(const char *headerBuf, uint64_t *imageSize, const char* image)
{
    return FsHvb::fsHvbFunc->CheckAndGetErofsSize(const char *headerBuf, uint64_t *imageSize, const char* image);
}

bool MockCheckandgetextheadersize(const int fd, uint64_t offset, uint64_t *imageSize, const char* image)
{
    return FsHvb::fsHvbFunc->CheckAndGetExtheaderSize(const int fd, uint64_t offset, uint64_t *imageSize, const char* image);
}

