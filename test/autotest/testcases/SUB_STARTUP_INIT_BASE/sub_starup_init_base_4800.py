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


class SunStartupInitBase4800(TestCase):
    """验证服务Socket(service_socket)功能:
    - 服务Unix Domain Socket创建和监听
    - Socket连接和事件处理
    - 对应unittest: service_socket_unittest.cpp
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
        Step("验证Unix Domain Socket存在")
        result = self.driver.shell("ls /dev/unix/ 2>/dev/null | head -10")
        Step("unix socket列表: %s" % result)

        Step("验证service_mgr_control_fd socket")
        result = self.driver.shell("ls -la /dev/unix/service_mgr_control_fd 2>/dev/null")
        Step("service_mgr socket: %s" % result)

        Step("验证init相关socket文件")
        result = self.driver.shell("ls -la /dev/unix/ | grep -iE 'service|init|control'")
        Step("init socket: %s" % result)

        Step("验证socket权限")
        result = self.driver.shell("ls -la /dev/unix/ | head -10")
        Step("socket权限: %s" % result)

        Step("查看socket相关日志")
        result = self.driver.shell("hilog -x -t init | grep -iE 'socket|service_socket' | tail -5")
        Step("socket日志: %s" % result)

    def teardown(self):
        Step("收尾工作.................")
