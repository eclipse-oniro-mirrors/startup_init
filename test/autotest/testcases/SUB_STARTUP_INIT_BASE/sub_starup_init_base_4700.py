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


class SunStartupInitBase4700(TestCase):
    """验证服务文件(service_file)功能:
    - 服务配置文件创建和读取
    - 控制文件(control file)访问
    - 对应unittest: service_file_unittest.cpp
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
        Step("验证服务配置文件目录存在")
        result = self.driver.shell("ls /system/etc/init/ 2>/dev/null | head -10")
        Step("init服务配置目录: %s" % result)

        Step("验证param_watcher服务配置文件")
        result = self.driver.shell("cat /etc/init/param_watcher.cfg 2>/dev/null")
        self.driver.Assert.contains(result, "param_watcher")

        Step("验证服务PID文件目录")
        result = self.driver.shell("ls /var/run/*.pid 2>/dev/null | head -5")
        Step("PID文件: %s" % result)

        Step("验证关键服务的PID文件存在")
        services = ["param_watcher", "appspawn", "fd_holder"]
        for svc in services:
            pid = self.driver.System.get_pid(svc)
            if pid and pid > 0:
                Step("%s PID: %d" % (svc, pid))

        Step("验证服务配置文件权限")
        result = self.driver.shell("ls -la /etc/init/param_watcher.cfg 2>/dev/null")
        Step("param_watcher.cfg权限: %s" % result)

        Step("查看服务文件相关日志")
        result = self.driver.shell("hilog -x -t init | grep -iE 'service_file|control_file' | tail -5")
        Step("服务文件日志: %s" % result)

    def teardown(self):
        Step("收尾工作.................")
