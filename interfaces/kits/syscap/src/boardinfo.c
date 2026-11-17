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
#include "beget_ext.h"
#include "parameter.h"
#include <stddef.h>
#include <stdint.h>
 
#define BOARD_SERIAL_LEN 256
#define BOARD_PARAM_NULL 401
 
const char *OH_BoardInfo_GetCpuId(void)
{
    return AclGetCpuId();
}
 
int32_t OH_BoardInfo_GetBoardSn(char *boardSn)
{
    if (boardSn == NULL) {
        BEGET_LOGE("GetBoardSn invalid param, boardSn is null");
        return BOARD_PARAM_NULL;
    }
    int ret = AclGetBoardSerial(boardSn, BOARD_SERIAL_LEN);
    if (ret != 0) {
        BEGET_LOGE("GetBoardSn failed, ret = %d", ret);
        return BOARD_INFO_PERMISSION_DENIED_CODE;
    }
    return 0;
}