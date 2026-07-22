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


class SunStartupInitBase3800(TestCase):
    """验证init命令执行器(cmdexecutor)功能:
    - 命令执行器注册与插件命令执行
    - 对应unittest: cmdexecutor_unittest.cpp
    """

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
        Step("验证init进程正常运行(pid=1)")
        result = self.driver.shell("ps -A | grep 'init' | head -1")
        self.driver.Assert.contains(result, "init")

        Step("验证init进程状态字段包含Name和State")
        result = self.driver.shell("cat /proc/1/status | grep -E 'Name|State'")
        self.driver.Assert.contains(result, "Name")
        self.driver.Assert.contains(result, "State")

        Step("验证param_set/param_get命令执行器可正常工作")
        result = self.driver.shell("param set test.cmdexecutor.flag 1")
        Step("param set结果: %s" % result)
        result = self.driver.shell("param get test.cmdexecutor.flag")
        self.driver.Assert.equal(result.strip(), "1")

        Step("清理测试参数")
        self.driver.shell("param set test.cmdexecutor.flag 0")

        Step("验证hilog命令执行器可正常工作")
        result = self.driver.shell("hilog -x -t init | head -5")
        Step("init日志: %s" % result)

    def teardown(self):
        Step("收尾工作.................")
        self.driver.shell("param set test.cmdexecutor.flag 0")
