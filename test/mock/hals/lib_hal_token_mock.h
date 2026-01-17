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
#ifndef APPSPAWN_HAL_TOKEN_MOCK_H
#define APPSPAWN_HAL_TOKEN_MOCK_H

int MockHalreadtoken(char *token, unsigned int len);

int MockHalwritetoken(const char *token, unsigned int len);

int MockHalgetackey(char *acKey, unsigned int len);

int MockHalgetprodid(char *productId, unsigned int len);

int MockHalgetprodkey(char *productKey, unsigned int len);

namespace OHOS {
namespace AppSpawn {
class HalToken {
public:
    virtual ~HalToken() = default;
    virtual int HalReadToken(char *token, unsigned int len) = 0;

    virtual int HalWriteToken(const char *token, unsigned int len) = 0;

    virtual int HalGetAcKey(char *acKey, unsigned int len) = 0;

    virtual int HalGetProdId(char *productId, unsigned int len) = 0;

    virtual int HalGetProdKey(char *productKey, unsigned int len) = 0;

public:
    static inline std::shared_ptr<HalToken> halTokenFunc = nullptr;
};

class HalTokenMock : public HalToken {
public:

    MOCK_METHOD(int, HalReadToken, (char *token, unsigned int len));

    MOCK_METHOD(int, HalWriteToken, (const char *token, unsigned int len));

    MOCK_METHOD(int, HalGetAcKey, (char *acKey, unsigned int len));

    MOCK_METHOD(int, HalGetProdId, (char *productId, unsigned int len));

    MOCK_METHOD(int, HalGetProdKey, (char *productKey, unsigned int len));

};
} // namespace AppSpawn
} // namespace OHOS

#endif // APPSPAWN_HAL_TOKEN_MOCK_H
