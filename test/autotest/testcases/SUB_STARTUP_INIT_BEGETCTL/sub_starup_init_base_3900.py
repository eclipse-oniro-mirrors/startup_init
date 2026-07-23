# -*- coding: utf-8 -*-
#
# Copyright (c) 2026 Huawei Device Co., Ltd.
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


class SunStartupInitBase3900(TestCase):

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
        wake = self.driver.Screen.is_on()
        time.sleep(0.3)
        if wake:
            self.driver.ScreenLock.unlock()
        else:
            self.driver.Screen.wake_up()
            self.driver.ScreenLock.unlock()
        self.driver.shell("power-shell timeout -o 86400000")

    def process(self):
        Step("读取系统属性const.product.name")
        result = self.driver.shell("param get const.product.name")
        result = result.replace("\n", "").replace(" ", "")
        Step("产品名称: " + result)
        self.driver.Assert.is_true(len(result) > 0)

        Step("读取系统属性const.product.model")
        result = self.driver.shell("param get const.product.model")
        result = result.replace("\n", "").replace(" ", "")
        Step("产品型号: " + result)
        self.driver.Assert.is_true(len(result) > 0)

        Step("读取系统属性const.product.manufacturer")
        result = self.driver.shell("param get const.product.manufacturer")
        result = result.replace("\n", "").replace(" ", "")
        Step("制造商: " + result)

        Step("读取系统属性const.product.soc")
        result = self.driver.shell("param get const.product.soc")
        result = result.replace("\n", "").replace(" ", "")
        Step("SoC型号: " + result)

        Step("系统属性校验完成")
        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        target_path = os.path.join(path, "testFile", "SUB_STARTUP_INIT_BASE")
        self.driver.Assert.is_true(os.path.exists(path))

    def teardown(self):
        Step("收尾工作.................")
