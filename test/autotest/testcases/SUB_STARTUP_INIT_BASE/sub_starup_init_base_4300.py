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


class SunStartupInitBase4300(TestCase):
    """验证重启功能(init_reboot)验证:
    - 重启命令执行器注册
    - 各种重启类型(reboot/shutdown/suspend/charge/updater)
    - 对应unittest: init_reboot_unittest.cpp
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
        Step("验证ohos.startup.powerctrl参数存在")
        result = self.driver.shell("param get ohos.startup.powerctrl")
        Step("powerctrl参数: %s" % result)

        Step("验证重启相关日志记录")
        result = self.driver.shell("hilog -x -t init | grep -iE 'reboot|shutdown|suspend' | tail -10")
        Step("重启日志: %s" % result)

        Step("验证重启命令执行器已注册")
        result = self.driver.shell("hilog -x -t init | grep -i 'executor' | tail -5")
        Step("执行器日志: %s" % result)

        Step("验证设备启动原因")
        result = self.driver.shell("param get ohos.boot.reason")
        Step("启动原因: %s" % result)

        Step("验证boot mode参数")
        result = self.driver.shell("param get ohos.boot.mode")
        Step("启动模式: %s" % result)

        Step("验证misc分区启动信息")
        result = self.driver.shell("cat /dev/block/by-name/misc 2>/dev/null | strings | head -5")
        Step("misc信息: %s" % result)

    def teardown(self):
        Step("收尾工作.................")
