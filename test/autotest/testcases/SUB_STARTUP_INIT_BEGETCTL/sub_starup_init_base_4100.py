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


class SunStartupInitBase4100(TestCase):

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
        Step("检查系统进程init")
        result = self.driver.shell("ps -ef | grep init | grep -v grep")
        self.driver.Assert.contains(result, "init")
        Step("init进程运行正常")

        Step("检查系统进程appspawn")
        result = self.driver.shell("ps -ef | grep appspawn | grep -v grep")
        self.driver.Assert.contains(result, "appspawn")
        Step("appspawn进程运行正常")

        Step("检查系统进程hiview")
        result = self.driver.shell("ps -ef | grep hiview | grep -v grep")
        self.driver.Assert.contains(result, "hiview")
        Step("hiview进程运行正常")

        Step("检查系统进程foundation")
        result = self.driver.shell("ps -ef | grep foundation | grep -v grep")
        self.driver.Assert.contains(result, "foundation")
        Step("foundation进程运行正常")

        Step("检查总进程数量")
        result = self.driver.shell("ps -ef | wc -l")
        count = int(result.strip())
        Step("当前进程总数: " + str(count))
        self.driver.Assert.is_true(count > 20)

        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        target_path = os.path.join(path, "testFile", "SUB_STARTUP_INIT_BASE")
        Step("系统进程校验完成")

    def teardown(self):
        Step("收尾工作.................")
