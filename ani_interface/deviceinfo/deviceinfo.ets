/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

loadLibrary("deviceinfo_ani")

class deviceInfo {
    deviceType: string = "";
    productSeries: string = "";
    productModel: string = "";
    ODID: string = "";
    sdkApiVersion: int;

    static get deviceType():string {
        return deviceInfo.GetDeviceType();
    }
    static get productSeries():string {
        return deviceInfo.GetProductSeries();
    }
    static get productModel():string {
        return deviceInfo.GetProductModel();
    }
    static get ODID():string {
        return deviceInfo.GetOdid();
    }
    static get sdkApiVersion():int {
        return deviceInfo.GetSdkApiVersion();
    }

    static native GetDeviceType(): string;
    static native GetProductSeries(): string;
    static native GetProductModel(): string;
    static native GetOdid(): string;
    static native GetSdkApiVersion(): int;
}