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

#ifndef INIT_PARAM_COMM_H
#define INIT_PARAM_COMM_H
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#define UDID_LEN 65
#define MAX_SERIAL_LEN 65
#define HASH_LENGTH 32
#define DEV_BUF_LENGTH 3
#define DEV_BUF_MAX_LENGTH 1024
#define DECIMAL 10
#define HEX 16

const char *GetProperty(const char *key, const char **paramHolder);

int StringToULL(const char *str, unsigned long long int *out);
int StringToLL(const char *str, long long int *out);
int HalGetParameter(const char *key, const char *def, char *value, unsigned int len);

const char *GetProductModel_(void);
const char *GetManufacture_(void);
const char *GetSerial_(void);
int GetDevUdid_(char *udid, int size);
int IsValidValue(const char *value, unsigned int len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif // STARTUP_PARAM_COMM_H
