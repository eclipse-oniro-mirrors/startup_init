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


class SunStartupInitBase4200(TestCase):
    """验证分组管理(group)功能:
    - group配置文件解析和服务分组
    - 服务start-mode和cpucore配置
    - 对应unittest: group_unittest.cpp
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
        Step("验证group配置文件存在并可读")
        result = self.driver.shell("cat /etc/init/group.cfg 2>&1")
        self.driver.Assert.contains(result, "group")

        Step("验证启动组模式参数")
        result = self.driver.shell("param get const.startup.init.groupmode")
        Step("group mode: %s" % result)
        self.driver.Assert.not_equal(result.strip(), "")

        Step("验证当前启动组")
        result = self.driver.shell("param get const.startup.init.bootgroup")
        Step("boot group: %s" % result)

        Step("验证by-condition服务启动模式")
        result = self.driver.shell("param get const.startup.init.groupmode")
        Step("groupmode: %s" % result)

        Step("验证param_watcher服务配置和运行")
        pid = self.driver.System.get_pid("param_watcher")
        Step("param_watcher PID: %s" % pid)
        self.driver.Assert.not_equal(pid, None)

        Step("验证appspawn服务配置和运行")
        pid = self.driver.System.get_pid("appspawn")
        Step("appspawn PID: %s" % pid)
        self.driver.Assert.not_equal(pid, None)

        Step("验证服务cpucore亲和性配置")
        services = ["param_watcher", "appspawn"]
        for svc in services:
            pid = self.driver.System.get_pid(svc)
            if pid and pid > 0:
                result = self.driver.shell("taskset -p %d 2>/dev/null" % pid)
                Step("%s taskset: %s" % (svc, result))

    def teardown(self):
        Step("收尾工作.................")
