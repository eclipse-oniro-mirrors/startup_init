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


/* Private guards are tested in C without changing production visibility. */
#ifndef LIBFS_HVB_TEST_ADAPTER_H
#define LIBFS_HVB_TEST_ADAPTER_H
#include "fs_hvb.h"
#ifdef __cplusplus
extern "C" {
#endif
enum hvb_errno HvbTestGetCurrVerifiedData(InitHvbType type, struct hvb_verified_data ***data);
int HvbTestGetHashStr(char *str, size_t size, InitHvbType type);
int HvbTestGetDigestStr(char *str, size_t size, InitHvbType type);
int HvbTestSha256(char *digest, uint32_t size, struct hvb_cert_data *certs, uint64_t count);
int HvbTestSm3(char *digest, uint32_t *size, struct hvb_cert_data *certs, uint64_t count);
int HvbTestHexToBin(char *bin, size_t size, char *str, size_t length);
int HvbTestFindVab(const char *name, HvbDeviceParam *param);
int HvbTestAddFec(struct hvb_cert *cert, char *path, char **cursor, char *end);
int HvbTestExtSetup(const char *name, const char *partition, char **path);
int64_t HvbTestImageSize(int fd, const char *name);
bool HvbTestIsExtPartition(const char *name, size_t length, size_t *index);
char *HvbTestExtPath(size_t index);
#ifdef __cplusplus
}
#endif
#endif
