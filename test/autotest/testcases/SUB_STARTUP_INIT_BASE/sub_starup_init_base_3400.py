# -*- coding: utf-8 -*-

#
# Copyright (c) 2024 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os
import time
from devicetest.core.test_case import TestCase, Step
from hypium import UiDriver


class SunStartupInitBase3400(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        TestCase.__init__(self, self.tag, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step(self.devices[0].device_id)
        device = self.driver.shell("param get const.product.model")
        device = device.replace("\n", "").replace(" ", "")
        device = str(device)
        Step(device)

        Step("解锁设备")
        wake = self.driver.Screen.is_on()
        time.sleep(0.5)
        if wake:
            self.driver.ScreenLock.unlock()
        else:
            self.driver.Screen.wake_up()
            self.driver.ScreenLock.unlock()
        self.driver.shell("power-shell timeout -o 86400000")

    def process(self):
        Step("测试设备指纹 ohos.boot.fingerprint")
        result = self.driver.shell("param get ohos.boot.fingerprint")
        fingerprint = result.strip()
        Step(f"设备指纹: {fingerprint}")

        Step("测试设备指纹不为空")
        self.driver.Assert.not_equal(fingerprint, "")

        Step("测试安全补丁版本 const.ohos.version.security_patch")
        result = self.driver.shell("param get const.ohos.version.security_patch")
        security_patch = result.strip()
        Step(f"安全补丁: {security_patch}")

        Step("测试安全补丁不为空")
        self.driver.Assert.not_equal(security_patch, "")

        Step("测试发布类型 const.ohos.version.release_type")
        result = self.driver.shell("param get const.ohos.version.release_type")
        release_type = result.strip()
        Step(f"发布类型: {release_type}")

        Step("测试发布类型不为空")
        self.driver.Assert.not_equal(release_type, "")

    def teardown(self):
        Step("收尾工作.................")