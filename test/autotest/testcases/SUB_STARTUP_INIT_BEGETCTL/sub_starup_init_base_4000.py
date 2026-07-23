# -*- coding: utf-8 -*-
#
# Copyright (c) 2026 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os
import time
from devicetest.core.test_case import TestCase, Step
from hypium import UiDriver


class SunStartupInitBase4000(TestCase):

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
        time.sleep(0.3)
        if wake:
            self.driver.ScreenLock.unlock()
        else:
            self.driver.Screen.wake_up()
            self.driver.ScreenLock.unlock()
        self.driver.shell("power-shell timeout -o 86400000")

    def process(self):
        Step("检查dmesg日志")
        self.driver.shell("dmesg -c")
        time.sleep(1)

        Step("写入内核日志")
        self.driver.shell("echo test_dmesg_4000 > /dev/kmsg")
        time.sleep(1)

        Step("读取dmesg日志匹配关键字")
        result = self.driver.shell("dmesg | grep test_dmesg_4000")
        self.driver.Assert.contains(result, "test_dmesg_4000")

        Step("检查dmesg日志级别")
        result = self.driver.shell("dmesg -l err")
        Step("错误级别日志已获取")

        Step("检查dmesg日志时间戳")
        result = self.driver.shell("dmesg -T | head -5")
        self.driver.Assert.is_true(len(result) > 0)

        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        target_path = os.path.join(path, "testFile", "SUB_STARTUP_INIT_BASE")
        Step("dmesg日志校验完成")

    def teardown(self):
        Step("收尾工作.................")
