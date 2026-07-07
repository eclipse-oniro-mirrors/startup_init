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


class SunStartupInitBase3000(TestCase):

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
        Step("测试系统版本信息 const.system.version")
        result = self.driver.shell("param get const.system.version")
        system_version = result.strip()
        Step(f"系统版本: {system_version}")

        Step("测试构建类型 const.product.build.type")
        result = self.driver.shell("param get const.product.build.type")
        build_type = result.strip()
        Step(f"构建类型: {build_type}")

        Step("测试构建用户 const.product.build.user")
        result = self.driver.shell("param get const.product.build.user")
        build_user = result.strip()
        Step(f"构建用户: {build_user}")

        Step("测试构建主机 const.product.build.host")
        result = self.driver.shell("param get const.product.build.host")
        build_host = result.strip()
        Step(f"构建主机: {build_host}")

        Step("测试构建时间 const.product.build.date")
        result = self.driver.shell("param get const.product.build.date")
        build_date = result.strip()
        Step(f"构建时间: {build_date}")

    def teardown(self):
        Step("收尾工作.................")