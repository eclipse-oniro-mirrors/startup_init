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


class SunStartupInitBase3700(TestCase):

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
        Step("测试设备时区设置 persist.sys.timezone")
        result = self.driver.shell("param get persist.sys.timezone")
        timezone = result.strip()
        Step(f"设备时区: {timezone}")

        Step("测试设备时区不为空")
        self.driver.Assert.not_equal(timezone, "")

        Step("测试设备时间格式 persist.sys.timeformat")
        result = self.driver.shell("param get persist.sys.timeformat")
        time_format = result.strip()
        Step(f"时间格式: {time_format}")

        Step("测试设备日期格式 persist.sys.dateformat")
        result = self.driver.shell("param get persist.sys.dateformat")
        date_format = result.strip()
        Step(f"日期格式: {date_format}")

        Step("测试设备名称 persist.sys.deviceName")
        result = self.driver.shell("param get persist.sys.deviceName")
        device_name = result.strip()
        Step(f"设备名称: {device_name}")

    def teardown(self):
        Step("收尾工作.................")