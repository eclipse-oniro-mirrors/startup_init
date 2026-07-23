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


class SunStartupInitBase4100(TestCase):
    """验证FD持有服务(fd_holder_service)功能:
    - fd_holder服务运行状态
    - fd持有和恢复机制
    - 对应unittest: fd_holder_service_unittest.cpp
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
        Step("验证fd_holder服务进程存在")
        pid = self.driver.System.get_pid("fd_holder")
        Step("fd_holder PID: %s" % pid)
        self.driver.Assert.not_equal(pid, None)
        self.driver.Assert.not_equal(pid, 0)

        Step("验证fd_holder进程状态正常")
        if pid and pid > 0:
            result = self.driver.shell("cat /proc/%d/status | grep -E 'Name|State|FDSize'" % pid)
            Step("fd_holder进程状态: %s" % result)
            self.driver.Assert.contains(result, "Name")
            self.driver.Assert.contains(result, "State")

        Step("验证fd_holder进程打开的文件描述符")
        if pid and pid > 0:
            result = self.driver.shell("ls /proc/%d/fd 2>/dev/null | wc -l" % pid)
            Step("fd_holder打开的FD数量: %s" % result)

        Step("查看fd_holder相关日志")
        result = self.driver.shell("hilog -x -t init | grep -i fd_holder | tail -5")
        Step("fd_holder日志: %s" % result)

    def teardown(self):
        Step("收尾工作.................")
