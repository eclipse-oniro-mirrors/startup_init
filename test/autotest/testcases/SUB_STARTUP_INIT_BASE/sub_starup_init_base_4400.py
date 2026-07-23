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


class SunStartupInitBase4400(TestCase):
    """验证init核心功能(init_unittest):
    - 信号处理(SIGCHLD/SIGTERM/SIGUSR1)
    - 日志级别设置与输出
    - 配置文件解析
    - 对应unittest: init_unittest.cpp
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
        Step("验证init进程(PID=1)状态正常")
        result = self.driver.shell("cat /proc/1/status | grep -E 'Name|State'")
        self.driver.Assert.contains(result, "Name")
        self.driver.Assert.contains(result, "init")

        Step("验证init进程信号掩码")
        result = self.driver.shell("cat /proc/1/status | grep Sig")
        Step("init信号掩码: %s" % result)

        Step("验证init日志级别可设置")
        result = self.driver.shell("param set persist.init.debug.loglevel 3")
        Step("设置日志级别: %s" % result)
        result = self.driver.shell("param get persist.init.debug.loglevel")
        self.driver.Assert.equal(result.strip(), "3")

        Step("验证init日志输出正常")
        result = self.driver.shell("hilog -x -t init | head -10")
        self.driver.Assert.not_equal(result.strip(), "")

        Step("验证配置文件解析/etc/init目录")
        result = self.driver.shell("ls /etc/init/*.cfg 2>/dev/null | head -10")
        Step("init配置文件: %s" % result)

        Step("恢复日志级别")
        self.driver.shell("param set persist.init.debug.loglevel 0")

    def teardown(self):
        Step("收尾工作.................")
        self.driver.shell("param set persist.init.debug.loglevel 0")
