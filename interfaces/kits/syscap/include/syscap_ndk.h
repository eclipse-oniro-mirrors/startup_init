/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef SYSCAP_NDK_H
#define SYSCAP_NDK_H

#include <stdbool.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief Queries whether a device supports a specified SystemCapability.
 * @return true - Supports the specified SystemCapability.
 *         false - The specified SystemCapability is not supported.
 */
bool canIUse(const char *cap);

/**
 * @brief determine whether the current operating system version is greater than or equal to the given value.
 *
 * @param majorVersion The major version number which betwen 1 and 999, such as 19 in api version 19.1.2
 * @param minorVersion The minor version number which betwen 0 and 999, such as 1 in api version 19.1.2
 * @param patchVersion The patch version number which betwen 0 and 999, such as 2 in api version 19.1.2
 * @return true - operating system version is greater than or equal to the given value
 *         false - operating system version is less than the given value or invalid api version
 * @since 19
 * @example given version is "19.1"
 * if (OH_IsApiVersionGreaterOrEqual(19, 1, 0)) {
 *    // Use 19.1 APIs.
 * } else {
 *    // Alternative code for earlier versions.
 * }
 */
bool OH_IsApiVersionGreaterOrEqual(int majorVersion, int minorVersion, int patchVersion);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif