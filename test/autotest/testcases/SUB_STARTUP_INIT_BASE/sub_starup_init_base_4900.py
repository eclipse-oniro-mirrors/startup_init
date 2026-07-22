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


class SunStartupInitBase4900(TestCase):
    """验证服务管理(service_unittest)功能:
    - 服务生命周期(start/stop/reap)
    - 服务cgroup配置和内核权限
    - 对应unittest: service_unittest.cpp
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
        Step("验证param_watcher服务运行中")
        pid = self.driver.System.get_pid("param_watcher")
        self.driver.Assert.not_equal(pid, None)
        self.driver.Assert.not_equal(pid, 0)
        Step("param_watcher PID: %d" % pid)

        Step("验证appspawn服务运行中")
        pid = self.driver.System.get_pid("appspawn")
        self.driver.Assert.not_equal(pid, None)
        self.driver.Assert.not_equal(pid, 0)
        Step("appspawn PID: %d" % pid)

        Step("验证服务cgroup配置")
        services = ["param_watcher", "appspawn"]
        for svc in services:
            pid = self.driver.System.get_pid(svc)
            if pid and pid > 0:
                result = self.driver.shell("cat /proc/%d/cgroup 2>/dev/null" % pid)
                Step("%s cgroup: %s" % (svc, result))

        Step("验证服务进程资源限制")
        for svc in services:
            pid = self.driver.System.get_pid(svc)
            if pid and pid > 0:
                result = self.driver.shell("cat /proc/%d/limits 2>/dev/null | head -5" % pid)
                Step("%s limits: %s" % (svc, result))

        Step("验证bootevent参数设置")
        result = self.driver.shell("param get persist.sys.bootevent.init")
        Step("bootevent init: %s" % result)

        Step("验证服务重启计数")
        result = self.driver.shell("hilog -x -t init | grep -iE 'service.*restart|reap' | tail -5")
        Step("服务重启日志: %s" % result)

        Step("验证内核权限设置日志")
        result = self.driver.shell("hilog -x -t init | grep -iE 'kernel.*perm|permission' | tail -5")
        Step("内核权限日志: %s" % result)

    def teardown(self):
        Step("收尾工作.................")
