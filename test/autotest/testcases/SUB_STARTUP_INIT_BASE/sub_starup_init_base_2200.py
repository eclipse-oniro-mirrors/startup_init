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


class SunStartupInitBase2200(TestCase):

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
        Step("查看fd_holder服务进程")
        result = self.driver.shell("ps -A | grep fd_holder")
        self.driver.Assert.contains(result, "fd_holder")
        pid = self.driver.System.get_pid("fd_holder")
        self.driver.Assert.equal(type(pid), int)
        result = self.driver.shell("cat /proc/%d/status | grep -E 'Name|State|SigCgt'" % pid)
        Step("fd_holder进程状态: %s" % result)

        Step("查看fd_holder配置文件")
        result = self.driver.shell("cat /etc/init/fd_holder.cfg")
        self.driver.Assert.contains(result, "fd_holder")

        Step("查看fd_holder进程打开的文件描述符")
        result = self.driver.shell("ls -la /proc/%d/fd/" % pid)
        Step(result)

        Step("查看param_watcher与fd_holder关联")
        result = self.driver.shell("ps -A | grep param_watcher")
        Step(result)
        pid_pw = self.driver.System.get_pid("param_watcher")
        if pid_pw and pid_pw > 0:
            result = self.driver.shell("ls -la /proc/%d/fd/" % pid_pw)
            Step(result)

        Step("查看fd持有事件相关日志")
        result = self.driver.shell("hilog -x -t init | grep -E 'fd_holder|FdHold' | tail -10")
        Step(result)

    def teardown(self):
        Step("收尾工作.................")
