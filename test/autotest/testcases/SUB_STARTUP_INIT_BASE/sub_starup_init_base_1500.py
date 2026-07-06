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


class SunStartupInitBase1500(TestCase):

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
        Step("查看已挂载文件系统")
        result = self.driver.shell("df -h")
        self.driver.Assert.contains(result, "/data")
        self.driver.Assert.contains(result, "/system")
        self.driver.Assert.contains(result, "/vendor")

        Step("查看挂载参数")
        result = self.driver.shell("mount | grep -E 'system|data|vendor'")
        Step(result)

        Step("查看块设备映射")
        result = self.driver.shell("ls -la /dev/block/by-name/")
        Step(result)

        Step("查看init挂载相关日志")
        result = self.driver.shell("hilog -x -t init | grep -E 'mount|fstab|partition'")
        Step(result)

        Step("查看fstab配置")
        result = self.driver.shell("cat /etc/fstab.* 2>/dev/null")
        Step(result)

    def teardown(self):
        Step("收尾工作.................")
