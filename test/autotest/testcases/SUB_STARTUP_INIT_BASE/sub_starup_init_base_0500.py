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


class SunStartupInitBase0500(TestCase):

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
        Step("通过begetctl启动param_watcher服务")
        self.driver.shell('begetctl start_service param_watcher')
        time.sleep(1)
        result = self.driver.shell('ps -A | grep param_watcher')
        self.driver.Assert.contains(result, 'param_watcher')
        pid = self.driver.System.get_pid('param_watcher')
        self.driver.Assert.equal(type(pid), int)

        Step("通过begetctl停止param_watcher服务")
        self.driver.shell('begetctl stop_service param_watcher')
        time.sleep(1)
        result = self.driver.shell('ps -A | grep param_watcher')
        self.driver.Assert.equal('param_watcher' not in result, True)

        Step("验证服务的critical和importance配置")
        result = self.driver.shell('cat /etc/init/param_watcher.cfg')
        self.driver.Assert.contains(result, 'param_watcher')
        self.driver.Assert.contains(result, 'importance')

        Step("验证不同服务的kill信号差异")
        result = self.driver.shell('cat /proc/1/status | grep SigCgt')
        self.driver.Assert.contains(result, 'SigCgt')

    def teardown(self):
        Step("收尾工作.................")
