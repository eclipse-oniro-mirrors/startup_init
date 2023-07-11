/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "init_eng.h"

#include <dirent.h>
#include <limits.h>
#include <sys/mount.h>

#include "plugin_adapter.h"
#include "init_cmds.h"
#include "init_utils.h"
#include "init_module_engine.h"
#include "securec.h"

#define ENG_SYSTEM_DEVICE_PATH "/dev/block/by-name/eng_system"
#define ENG_CHIPSET_DEVICE_PATH "/dev/block/by-name/eng_chipset"

ENG_STATIC bool IsFileExistWithType(const char *file, FileType type)
{
    bool isExist = false;
    struct stat st = {};
    if (lstat(file, &st) == 0) {
        switch (type) {
            case TYPE_DIR:
                if (S_ISDIR(st.st_mode)) {
                    isExist = true;
                }
                break;
            case TYPE_REG:
                if (S_ISREG(st.st_mode)) {
                    isExist = true;
                }
                break;
            case TYPE_LINK:
                if (S_ISLNK(st.st_mode)) {
                    isExist = true;
                }
                break;
            case TYPE_ANY: // fallthrough
            default:
                isExist = true;
                break;
        }
    }
    return isExist;
}

static bool IsRegularFile(const char *file)
{
    return file == NULL ? false : IsFileExistWithType(file, TYPE_REG);
}

static bool IsExistFile(const char *file)
{
    return file == NULL ? false : IsFileExistWithType(file, TYPE_ANY);
}

ENG_STATIC void BuildMountCmd(char *buffer, size_t len, const char *mp, const char *dev, const char *fstype)
{
    int ret = snprintf_s(buffer, len, len - 1, "%s %s %s ro barrier=1",
        fstype, dev, mp);
    if (ret == -1) {
        *buffer = '\0';
    }
}

ENG_STATIC void MountEngPartitions(void)
{
    char mountCmd[MOUNT_CMD_MAX_LEN] = {};
   // Mount eng_system
    BuildMountCmd(mountCmd, MOUNT_CMD_MAX_LEN, "/eng_system",
        "/dev/block/by-name/eng_system", "ext4");
    WaitForFile(ENG_SYSTEM_DEVICE_PATH, WAIT_MAX_SECOND);
    int cmdIndex = 0;
    (void)GetMatchCmd("mount ", &cmdIndex);
    DoCmdByIndex(cmdIndex, mountCmd, NULL);

   // Mount eng_chipset
    BuildMountCmd(mountCmd, MOUNT_CMD_MAX_LEN, "/eng_chipset",
        "/dev/block/by-name/eng_chipset", "ext4");
    WaitForFile(ENG_CHIPSET_DEVICE_PATH, WAIT_MAX_SECOND);
    DoCmdByIndex(cmdIndex, mountCmd, NULL);
}

ENG_STATIC void BindMountFile(const char *source, const char *target)
{
    char targetFullPath[PATH_MAX] = {};
    const char *p = source;
    char *q = NULL;
    const char *end = source + strlen(source);

    if (*p != '/') { // source must start with '/'
        return;
    }

    // Get next '/'
    q = strchr(p + 1, '/');
    if (q == NULL) {
        PLUGIN_LOGI("path \' %s \' without extra slash, ignore it", source);
        return;
    }

    if (*(end - 1) == '/') {
        PLUGIN_LOGI("path \' %s \' ends with slash, ignore it", source);
        return;
    }
    // OK, now get sub dir and combine it with target
    int ret = snprintf_s(targetFullPath, PATH_MAX, PATH_MAX - 1, "%s%s", strcmp(target, "/") == 0 ? "" : target, q);
    if (ret == -1) {
        PLUGIN_LOGE("Failed to build target path");
        return;
    }
    PLUGIN_LOGI("target full path is %s", targetFullPath);
    if (IsRegularFile(targetFullPath)) {
        if (mount(source, targetFullPath, NULL, MS_BIND, NULL) != 0) {
            PLUGIN_LOGE("Failed to bind mount %s to %s, err = %d", source, targetFullPath, errno);
        } else {
            PLUGIN_LOGI("Bind mount %s to %s done", source, targetFullPath);
        }
    } else {
        if (!IsExistFile(targetFullPath)) {
            if (symlink(source, targetFullPath) < 0) {
                PLUGIN_LOGE("Failed to link %s to %s, err = %d", source, targetFullPath, errno);
            }
        } else {
            PLUGIN_LOGW("%s without expected type, skip overlay", targetFullPath);
        }
    }
}

ENG_STATIC void DebugFilesOverlay(const char *source, const char *target)
{
    DIR *dir = NULL;
    struct dirent *de = NULL;

    if ((dir = opendir(source)) == NULL) {
        PLUGIN_LOGE("Open path \' %s \' failed. err = %d", source, errno);
        return;
    }
    int dfd = dirfd(dir);
    char srcPath[PATH_MAX] = {};
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.') {
            continue;
        }
        if (snprintf_s(srcPath, PATH_MAX, PATH_MAX - 1, "%s/%s", source, de->d_name) == -1) {
            PLUGIN_LOGE("Failed to build path for overlaying");
            break;
        }

        // Determine file type
        struct stat st = {};
        if (fstatat(dfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            DebugFilesOverlay(srcPath, target);
        } else if (S_ISREG(st.st_mode)) {
            BindMountFile(srcPath, target);
        } else { // Ignore any other file types
            PLUGIN_LOGI("Ignore %s while overlaying", srcPath);
        }
    }
    closedir(dir);
    dir = NULL;
}

ENG_STATIC void EngineerOverlay(void)
{
    PLUGIN_LOGI("system overlay...");
    DebugFilesOverlay("/eng_system", "/");
    PLUGIN_LOGI("vendor overlay...");
    DebugFilesOverlay("/eng_chipset", "/chipset");
}

MODULE_CONSTRUCTOR(void)
{
    PLUGIN_LOGI("Start eng mode now ...");
    MountEngPartitions();
    EngineerOverlay();
}
