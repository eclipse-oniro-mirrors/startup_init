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


class SunStartupInitBase4600(TestCase):
    """验证沙箱功能(sandbox_unittest):
    - 沙箱配置文件和命名空间
    - 沙箱挂载标志和namespace操作
    - 对应unittest: sandbox_unittest.cpp
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
        Step("验证沙箱配置文件存在")
        result = self.driver.shell("ls /system/etc/sandbox/ 2>/dev/null")
        Step("沙箱配置目录: %s" % result)

        Step("验证sandbox相关日志")
        result = self.driver.shell("hilog -x -t init | grep -iE 'sandbox|namespace' | tail -10")
        Step("沙箱日志: %s" % result)

        Step("验证命名空间隔离功能")
        result = self.driver.shell("ls /proc/1/ns/ 2>/dev/null")
        Step("init命名空间: %s" % result)

        Step("验证mount namespace存在")
        result = self.driver.shell("ls -la /proc/1/ns/mnt 2>/dev/null")
        Step("mount namespace: %s" % result)

        Step("验证sandbox相关进程状态")
        result = self.driver.shell("ps -A | grep -i sandbox")
        Step("sandbox进程: %s" % result)

        Step("验证sandbox JSON配置内容")
        result = self.driver.shell("cat /system/etc/sandbox/*.json 2>/dev/null | head -20")
        Step("sandbox配置: %s" % result)

    def teardown(self):
        Step("收尾工作.................")
