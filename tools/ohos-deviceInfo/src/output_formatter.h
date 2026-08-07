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

#ifndef OHOS_DEVICE_INFO_OUTPUT_FORMATTER_H
#define OHOS_DEVICE_INFO_OUTPUT_FORMATTER_H

#include <cstdio>
#include <string>

#include <cJSON.h>

#ifndef CLI_LOG
#define CLI_LOG(fmt, ...) do { \
    fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__); \
} while (0)
#endif

#ifndef CLI_ERROR
#define CLI_ERROR(fmt, ...) do { \
    fprintf(stdout, "[ERROR] " fmt "\n", ##__VA_ARGS__); \
} while (0)
#endif

int OutputSuccess(cJSON *data);
int OutputError(const std::string &code, const std::string &message, const std::string &suggestion);

#endif  // OHOS_DEVICE_INFO_OUTPUT_FORMATTER_H
