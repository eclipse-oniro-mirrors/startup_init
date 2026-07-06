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


class SunStartupInitBase0700(TestCase):

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
        Step("设置init日志级别为DEBUG并验证")
        current_level = self.driver.shell("param get persist.init.debug.loglevel")
        Step("当前日志级别: %s" % current_level)
        self.driver.shell("param set persist.init.debug.loglevel 0")
        level = self.driver.shell("param get persist.init.debug.loglevel")
        self.driver.Assert.equal(level.split()[0].strip(), "0")
        self.driver.shell("param set persist.init.debug.loglevel 3")
        restored = self.driver.shell("param get persist.init.debug.loglevel")
        self.driver.Assert.equal(restored.split()[0].strip(), "3")

        Step("验证init配置文件按优先级解析")
        result = self.driver.shell("ls /etc/init/*.cfg")
        self.driver.Assert.contains(result, ".cfg")
        result = self.driver.shell("begetctl dump_service param_watcher")
        self.driver.Assert.contains(result, "param_watcher")

        Step("验证系统挂载和设备节点")
        result = self.driver.shell("df -h")
        self.driver.Assert.contains(result, "/data")
        result = self.driver.shell("ls -la /dev/ | head -30")
        Step(result)

    def teardown(self):
        Step("收尾工作.................")
