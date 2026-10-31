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


#ifndef LIBFS_HVB_MOCK_H
#define LIBFS_HVB_MOCK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HVB_FAIL_NONE,
    HVB_FAIL_MALLOC,
    HVB_FAIL_CALLOC,
    HVB_FAIL_MEMCPY,
    HVB_FAIL_SPRINTF,
    HVB_FAIL_SNPRINTF,
    HVB_FAIL_OPEN,
    HVB_FAIL_READ,
    HVB_FAIL_LSEEK64,
    HVB_FAIL_BASENAME,
} HvbMockFailure;

void HvbMockReset(void);
void HvbMockFailAt(HvbMockFailure function, unsigned int call);
unsigned int HvbMockFailureHits(void);
unsigned int HvbMockOpenCount(void);
unsigned int HvbMockCloseCount(void);

#ifdef __cplusplus
}
#endif
#endif
