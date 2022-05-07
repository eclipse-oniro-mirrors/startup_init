/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "param_utils.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "init_utils.h"

void CheckAndCreateDir(const char *fileName)
{
#ifndef __LITEOS_M__
    if (fileName == NULL || *fileName == '\0') {
        return;
    }
    char *path = strndup(fileName, strrchr(fileName, '/') - fileName);
    if (path == NULL) {
        return;
    }
    if (access(path, F_OK) == 0) {
        free(path);
        return;
    }
    MakeDirRecursive(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    free(path);
#endif
}

int SpliteString(char *line, const char *exclude[], uint32_t count,
    int (*result)(const uint32_t *context, const char *name, const char *value), const uint32_t *context)
{
    // Skip spaces
    char *name = line;
    while (isspace(*name) && (*name != '\0')) {
        name++;
    }
    // Empty line or Comment line
    if (*name == '\0' || *name == '#') {
        return 0;
    }

    char *value = name;
    // find the first delimiter '='
    while (*value != '\0') {
        if (*value == '=') {
            (*value) = '\0';
            value = value + 1;
            break;
        }
        value++;
    }

    // Skip spaces
    char *tmp = name;
    while ((tmp < value) && (*tmp != '\0')) {
        if (isspace(*tmp)) {
            (*tmp) = '\0';
            break;
        }
        tmp++;
    }

    // empty name, just ignore this line
    if (*value == '\0') {
        return 0;
    }

    // Filter excluded parameters
    for (uint32_t i = 0; i < count; i++) {
        if (strncmp(name, exclude[i], strlen(exclude[i])) == 0) {
            return 0;
        }
    }

    // Skip spaces for value
    while (isspace(*value) && (*value != '\0')) {
        value++;
    }

    // Trim the ending spaces of value
    char *pos = value + strlen(value);
    pos--;
    while (isspace(*pos) && pos > value) {
        (*pos) = '\0';
        pos--;
    }

    // Strip starting and ending " for value
    if ((*value == '"') && (pos > value) && (*pos == '"')) {
        value = value + 1;
        *pos = '\0';
    }
    return result(context, name, value);
}

static char *BuildKey(const char *format, ...)
{
    const size_t buffSize = 1024;  // 1024 for format key
    char *buffer = malloc(buffSize);
    PARAM_CHECK(buffer != NULL, return NULL, "Failed to malloc for format");
    va_list vargs;
    va_start(vargs, format);
    int len = vsnprintf_s(buffer, buffSize, buffSize - 1, format, vargs);
    va_end(vargs);
    if (len > 0 && (size_t)len < buffSize) {
        buffer[len] = '\0';
        for (int i = 0; i < len; i++) {
            if (buffer[i] == '|') {
                buffer[i] = '\0';
            }
        }
        return buffer;
    }
    return NULL;
}

char *GetServiceCtrlName(const char *name, const char *value)
{
    static char *ctrlParam[] = {
        "ohos.ctl.start",
        "ohos.ctl.stop"
    };
    static char *installParam[] = {
        "ohos.servicectrl."
    };
    static char *powerCtrlArg[][2] = {
        {"reboot,shutdown", "reboot.shutdown"},
        {"reboot,updater", "reboot.updater"},
        {"reboot,flashd", "reboot.flashd"},
        {"reboot", "reboot"},
    };
    char *key = NULL;
    if (strcmp("ohos.startup.powerctrl", name) == 0) {
        for (size_t i = 0; i < ARRAY_LENGTH(powerCtrlArg); i++) {
            if (strncmp(value, powerCtrlArg[i][0], strlen(powerCtrlArg[i][0])) == 0) {
                return BuildKey("%s%s", OHOS_SERVICE_CTRL_PREFIX, powerCtrlArg[i][1]);
            }
        }
        return key;
    }
    for (size_t i = 0; i < ARRAY_LENGTH(ctrlParam); i++) {
        if (strcmp(name, ctrlParam[i]) == 0) {
            return BuildKey("%s%s", OHOS_SERVICE_CTRL_PREFIX, value);
        }
    }

    for (size_t i = 0; i < ARRAY_LENGTH(installParam); i++) {
        if (strncmp(name, installParam[i], strlen(installParam[i])) == 0) {
            return BuildKey("%s.%s", name, value);
        }
    }
    return key;
}