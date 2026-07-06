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


class SunStartupInitBase2000(TestCase):

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
        Step("查看init进程状态和信号掩码")
        result = self.driver.shell("cat /proc/1/status | grep -E 'Name|State|SigCgt|SigBlk'")
        self.driver.Assert.contains(result, "Name:	init")
        Step("init进程状态: %s" % result)

        Step("验证SIGCHLD子进程退出信号处理")
        result = self.driver.shell("ps -A | grep defunct")
        self.driver.Assert.equal('defunct' not in result, True)

        Step("查看init进程的子进程列表")
        result = self.driver.shell("ps -A --ppid 1 | head -20")
        Step(result)

        Step("查看信号处理相关日志")
        result = self.driver.shell("hilog -x -t init | grep -E 'signal' | tail -10")
        Step(result)

    def teardown(self):
        Step("收尾工作.................")
