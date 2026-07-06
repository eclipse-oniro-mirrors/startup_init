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


class SunStartupInitBase2300(TestCase):

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
        Step("测试 const.product.devicetype 参数")
        result = self.driver.shell("param get const.product.devicetype")
        self.driver.Assert.not_equal(result.strip(), "")

        Step("测试 const.product.brand 参数")
        result = self.driver.shell("param get const.product.brand")
        self.driver.Assert.not_equal(result.strip(), "")

        Step("测试 const.product.manufacturer 参数")
        result = self.driver.shell("param get const.product.manufacturer")
        self.driver.Assert.not_equal(result.strip(), "")

        Step("测试 const.product.hardwaremodel 参数")
        result = self.driver.shell("param get const.product.hardwaremodel")
        self.driver.Assert.not_equal(result.strip(), "")

        Step("测试 const.product.hardwareprofile 参数")
        result = self.driver.shell("param get const.product.hardwareprofile")
        self.driver.Assert.not_equal(result.strip(), "")

    def teardown(self):
        Step("收尾工作.................")