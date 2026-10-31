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


/*
 * Compile the unmodified implementation once, exposing only typed test entry
 * points for guards that its public callers cannot reach. No static rewriting.
 */
#include "../../../interfaces/innerkits/fs_manager/libfs_hvb/fs_hvb.c"
#include "libfs_hvb_test_adapter.h"

enum hvb_errno HvbTestGetCurrVerifiedData(InitHvbType type, struct hvb_verified_data ***data)
{
    return GetCurrVerifiedData(type, data);
}
int HvbTestGetHashStr(char *str, size_t size, InitHvbType type)
{
    return FsHvbGetHashStr(str, size, type);
}
int HvbTestGetDigestStr(char *str, size_t size, InitHvbType type)
{
    return FsHvbGetCertDigstStr(str, size, type);
}
int HvbTestSha256(char *digest, uint32_t size, struct hvb_cert_data *certs, uint64_t count)
{
    return FsHvbComputeSha256(digest, size, certs, count);
}
int HvbTestSm3(char *digest, uint32_t *size, struct hvb_cert_data *certs, uint64_t count)
{
    return FsHvbComputeSM3Hash(digest, size, certs, count);
}
int HvbTestHexToBin(char *bin, size_t size, char *str, size_t length)
{
    return FsHvbHexStrToBin(bin, size, str, length);
}
int HvbTestFindVab(const char *name, HvbDeviceParam *param)
{
    return FsHvbFindVabPartition(name, param);
}
int HvbTestAddFec(struct hvb_cert *cert, char *path, char **cursor, char *end)
{
    return FsHvbVerityTargetAddFecArgs(cert, path, cursor, end);
}
int HvbTestExtSetup(const char *name, const char *partition, char **path)
{
    return FsExtHvbSetupHashtree(name, partition, path);
}
