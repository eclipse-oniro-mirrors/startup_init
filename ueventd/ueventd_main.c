/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <limits.h>
#include <poll.h>
#include <unistd.h>
#include <stdbool.h>
#include "ueventd.h"
#include "ueventd_read_cfg.h"
#include "ueventd_socket.h"
#define INIT_LOG_TAG "ueventd"
#include "init_log.h"
#include "init_socket.h"
#include "parameter.h"

static bool IsComplete()
{
    static bool complete = false;
    if (complete) {
        return true;
    }
    char enable[8] = {0};
    int ret = GetParameter("bootevent.boot.completed", "", enable, sizeof(enable));
    if (ret != 0) {
        INIT_LOGE("Failed to get param value");
        return false;
    }
    if (strcmp(enable, "true") == 0) {
        INIT_LOGI("boot completed");
        complete = true;
        return true;
    }
    return false;
}

static void PollUeventdSocketTimeout(int ueventSockFd, bool ondemand)
{
    struct pollfd pfd = {};
    pfd.events = POLLIN;
    pfd.fd = ueventSockFd;
    int timeout = ondemand ? UEVENTD_POLL_TIME : -1;
    int ret = -1;

    while (1) {
        pfd.revents = 0;
        ret = poll(&pfd, 1, timeout);
        if (ret == 0) {
            if (IsComplete()) {
                INIT_LOGI("poll ueventd socket timeout, ueventd exit");
                return;
            }
            INIT_LOGI("poll ueventd socket timeout, but init not complete");
        } else if (ret < 0) {
            INIT_LOGE("Failed to poll ueventd socket!");
            return;
        }
        if (pfd.revents & (POLLIN | POLLERR)) {
            ProcessUevent(ueventSockFd, NULL, 0); // Not require boot devices
        }
    }
}

static int UeventdRetrigger(void)
{
    const char *ueventdConfigs[] = {"/etc/ueventd.config", "/vendor/etc/ueventd.config", NULL};
    int i = 0;
    while (ueventdConfigs[i] != NULL) {
        ParseUeventdConfigFile(ueventdConfigs[i++]);
    }
    int ueventSockFd = UeventdSocketInit();
    if (ueventSockFd < 0) {
        INIT_LOGE("Failed to create uevent socket!");
        return -1;
    }
    RetriggerUevent(ueventSockFd, NULL, 0); // Not require boot devices
    return 0;
}

static int UeventdDaemon(int listen_only)
{
    // start log
    EnableInitLog(INIT_INFO);
    const char *ueventdConfigs[] = {"/etc/ueventd.config", "/vendor/etc/ueventd.config", NULL};
    int i = 0;
    while (ueventdConfigs[i] != NULL) {
        ParseUeventdConfigFile(ueventdConfigs[i++]);
    }
    bool ondemand = true;
    int ueventSockFd = GetControlSocket("ueventd");
    if (ueventSockFd < 0) {
        INIT_LOGW("Failed to get uevent socket, try to create");
        ueventSockFd = UeventdSocketInit();
        ondemand = false;
    }
    if (ueventSockFd < 0) {
        INIT_LOGE("Failed to create uevent socket!");
        return -1;
    }
    if (!listen_only && access(UEVENTD_FLAG, F_OK)) {
        INIT_LOGI("Ueventd started, trigger uevent");
        RetriggerUevent(ueventSockFd, NULL, 0); // Not require boot devices
        int fd = open(UEVENTD_FLAG, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            INIT_LOGE("Failed to create ueventd flag!");
            return -1;
        }
        (void)close(fd);
    } else {
        INIT_LOGI("ueventd start to process uevent message");
        ProcessUevent(ueventSockFd, NULL, 0); // Not require boot devices
    }
    PollUeventdSocketTimeout(ueventSockFd, ondemand);
    CloseUeventConfig();
    return 0;
}

static int UeventdEarlyBoot(void)
{
    int ueventSockFd = UeventdSocketInit();
    if (ueventSockFd < 0) {
        return -1;
    }

    char *devices[] = {
        "/dev/block/vdb",
        "/dev/block/vdc"
    };

    RetriggerUevent(ueventSockFd, devices, 2);
    close(ueventSockFd);
    return 0;
}

#define UEVENTD_MODE_DEAMON     0
#define UEVENTD_MODE_EARLY_BOOT 1
#define UEVENTD_MODE_RETRIGGER  2
#define UEVENTD_MODE_LISTEN     3

static void usage(const char *name)
{
    printf("Usage: %s [OPTION]\n"
           "Listening kernel uevent to create device node.\n"
           "It will read configs from {/,/system,/chipset}/etc/ueventd.config.\n\n"
           "The options may be used to set listening mode.\n"
           "    -d, --daemon          working in deamon mode(default mode)\n"
           "    -b, --boot            working in early booting mode, create required device nodes\n"
           "    -l, --listen          listen in verbose mode\n"
           "    -r, --retrigger       retrigger all uevents\n"
           "    -v, --verbose         log level\n"
           "    -h, --help            print this help info\n", name);
}

static void UeventdLogPrint(int logLevel, uint32_t domain, const char *tag, const char *fmt, va_list vargs)
{
    if (logLevel < GetInitLogLevel()) {
        return;
    }
    vprintf(fmt, vargs);
    printf("\n");
}

int main(int argc, char *argv[])
{
    int opt;
    const char *config;
    int daemon = UEVENTD_MODE_DEAMON;

    while ((opt = getopt(argc, argv, "drblv:h")) != -1) {
        switch (opt) {
            case 'd':
                daemon = UEVENTD_MODE_DEAMON;
                break;
            case 'r':
                SetInitCommLog(UeventdLogPrint);
                daemon = UEVENTD_MODE_RETRIGGER;
                break;
            case 'b':
                SetInitCommLog(UeventdLogPrint);
                daemon = UEVENTD_MODE_EARLY_BOOT;
                break;
            case 'v':
                EnableInitLog(atoi(optarg));
                SetInitCommLog(UeventdLogPrint);
                break;
            case 'l':
                EnableInitLog(0);
                SetInitCommLog(UeventdLogPrint);
                daemon = UEVENTD_MODE_LISTEN;
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-t nsecs] [-n] name\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    config = NULL;
    if (optind >= argc) {
        config = argv[optind];
    }

    if (daemon == UEVENTD_MODE_DEAMON) {
        return UeventdDaemon(0);
    } else if (daemon == UEVENTD_MODE_RETRIGGER) {
        return UeventdRetrigger();
    } else if (daemon == UEVENTD_MODE_LISTEN) {
        return UeventdDaemon(1);
    } else {
        UeventdEarlyBoot();
    }

    return 0;
}
