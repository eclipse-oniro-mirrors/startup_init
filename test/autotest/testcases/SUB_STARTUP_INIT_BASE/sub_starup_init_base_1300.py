#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2025 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import time
from devicetest.core.test_case import TestCase, Step
from hypium import UiDriver


class SunStartupInitBase1300(TestCase):

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
        time.sleep(0.5)
        if wake:
            self.driver.ScreenLock.unlock()
        else:
            self.driver.Screen.wake_up()
            self.driver.ScreenLock.unlock()
        self.driver.shell("power-shell timeout -o 86400000")

    def process(self):
        Step("验证begetctl控制通道")
        result = self.driver.shell("begetctl -h")
        self.driver.Assert.contains(result, "Usage")

        Step("dump所有服务信息")
        result = self.driver.shell("begetctl dump_all")
        Step(result)

        Step("dump指定服务信息")
        result = self.driver.shell("begetctl dump_service param_watcher")
        self.driver.Assert.contains(result, "param_watcher")
        result = self.driver.shell("begetctl dump_service appspawn 2>&1")
        Step(result)

        Step("验证sandbox控制")
        result = self.driver.shell("begetctl sandbox system 2>&1")
        Step(result)

        Step("验证模块管理")
        result = self.driver.shell("begetctl modulemgr list 2>&1")
        Step(result)

    def teardown(self):
        Step("收尾工作.................")
