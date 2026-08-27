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

#include "output_formatter.h"

#include <cstdlib>
#include <cstring>

int OutputSuccess(cJSON *data)
{
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "type", "result");
    cJSON_AddStringToObject(response, "status", "success");
    cJSON_AddItemToObject(response, "data", data);
    char *jsonStr = cJSON_PrintUnformatted(response);
    if (jsonStr != nullptr) {
        (void)fprintf(stdout, "%s\n", jsonStr);
        free(jsonStr);
    } else {
        CLI_LOG("Failed to serialize success response: out of memory");
    }
    cJSON_Delete(response);
    return 0;
}

int OutputError(const std::string &code, const std::string &message, const std::string &suggestion)
{
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "type", "result");
    cJSON_AddStringToObject(response, "status", "failed");
    cJSON_AddStringToObject(response, "errCode", code.c_str());
    cJSON_AddStringToObject(response, "errMsg", message.c_str());
    cJSON_AddStringToObject(response, "suggestion", suggestion.c_str());
    char *jsonStr = cJSON_PrintUnformatted(response);
    if (jsonStr != nullptr) {
        (void)fprintf(stdout, "%s\n", jsonStr);
        free(jsonStr);
    } else {
        CLI_LOG("Failed to serialize error response: out of memory");
    }
    cJSON_Delete(response);
    return 1;
}
