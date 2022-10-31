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

#ifndef _PLUGIN_BOOTCHART_H
#define _PLUGIN_BOOTCHART_H
#include <pthread.h>
#include <stdint.h>

#define DEFAULT_BUFFER 2048
typedef struct {
    int start;
    int stop;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    pthread_t threadId;
    uint32_t bufferSize;
    char buffer[DEFAULT_BUFFER];
} BootchartCtrl;

#ifdef STARTUP_INIT_TEST
#define BOOTCHART_STATIC
#else
#define BOOTCHART_STATIC static
#endif

#endif /* _PLUGIN_BOOTCHART_H */
