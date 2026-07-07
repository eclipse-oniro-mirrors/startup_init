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


class SunStartupInitBase3100(TestCase):

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
        Step("测试内核版本 ohos.boot.kernel")
        result = self.driver.shell("param get ohos.boot.kernel")
        kernel_version = result.strip()
        Step(f"内核版本: {kernel_version}")

        Step("测试内核版本不为空")
        self.driver.Assert.not_equal(kernel_version, "")

        Step("测试启动时间 ohos.boot.time")
        result = self.driver.shell("param get ohos.boot.time")
        boot_time = result.strip()
        Step(f"启动时间: {boot_time}")

        Step("测试启动时间 init 阶段")
        result = self.driver.shell("param get ohos.boot.time.init")
        init_time = result.strip()
        Step(f"Init阶段时间: {init_time}")

        Step("测试启动时间参数是否为数字")
        if init_time:
            self.driver.Assert.equal(init_time.split()[0].isdigit(), True)

    def teardown(self):
        Step("收尾工作.................")