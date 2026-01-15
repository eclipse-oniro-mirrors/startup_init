/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include "lib_hal_token_mock.h"

using namespace OHOS::AppSpawn;
int MockHalreadtoken(char *token, unsigned int len)
{
    return HalToken::halTokenFunc->HalReadToken(char *token, unsigned int len);
}

int MockHalwritetoken(const char *token, unsigned int len)
{
    return HalToken::halTokenFunc->HalWriteToken(const char *token, unsigned int len);
}

int MockHalgetackey(char *acKey, unsigned int len)
{
    return HalToken::halTokenFunc->HalGetAcKey(char *acKey, unsigned int len);
}

int MockHalgetprodid(char *productId, unsigned int len)
{
    return HalToken::halTokenFunc->HalGetProdId(char *productId, unsigned int len);
}

int MockHalgetprodkey(char *productKey, unsigned int len)
{
    return HalToken::halTokenFunc->HalGetProdKey(char *productKey, unsigned int len);
}

