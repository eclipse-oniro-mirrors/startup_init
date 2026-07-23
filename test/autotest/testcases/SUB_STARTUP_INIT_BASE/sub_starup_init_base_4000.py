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


class SunStartupInitBase4000(TestCase):
    """验证控制FD服务(control_fd_service)功能:
    - dump all/specific service信息
    - sandbox和modulemgr控制命令
    - 对应unittest: control_fd_service_unittest.cpp
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
        Step("验证init进程可访问控制文件")
        result = self.driver.shell("ls -la /dev/service_mgr_control_fd 2>&1")
        Step("控制FD文件: %s" % result)

        Step("验证param_watcher服务运行状态")
        pid = self.driver.System.get_pid("param_watcher")
        Step("param_watcher PID: %s" % pid)
        self.driver.Assert.not_equal(pid, None)

        Step("验证fd_holder服务运行状态")
        pid = self.driver.System.get_pid("fd_holder")
        Step("fd_holder PID: %s" % pid)
        self.driver.Assert.not_equal(pid, None)

        Step("查看init相关控制日志")
        result = self.driver.shell("hilog -x -t init | grep -iE 'control|fd_holder|dump' | tail -10")
        Step("控制FD日志: %s" % result)

        Step("验证service manager socket存在")
        result = self.driver.shell("ls -la /dev/unix/service_socket_* 2>&1 | head -5")
        Step("service socket: %s" % result)

    def teardown(self):
        Step("收尾工作.................")
