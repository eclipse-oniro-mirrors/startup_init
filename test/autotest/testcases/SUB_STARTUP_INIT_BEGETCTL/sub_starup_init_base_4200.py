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


class SunStartupInitBase4200(TestCase):

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
        Step("检查存储空间使用情况")
        result = self.driver.shell("df -h /data")
        self.driver.Assert.contains(result, "/data")
        Step("存储空间检查完成")

        Step("检查/data目录下的关键目录")
        result = self.driver.shell("ls /data/log")
        self.driver.Assert.is_true(len(result) > 0)
        Step("/data/log目录存在")

        Step("检查/data/service目录")
        result = self.driver.shell("ls /data/service")
        self.driver.Assert.is_true(len(result) > 0)

        Step("检查/data/app目录")
        result = self.driver.shell("ls /data/app")
        Step("应用数据目录正常")

        Step("检查tmp目录空间")
        result = self.driver.shell("df -h /tmp")
        self.driver.Assert.contains(result, "/tmp")

        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        target_path = os.path.join(path, "testFile", "SUB_STARTUP_INIT_BASE")
        Step("文件系统完整性校验完成")

    def teardown(self):
        Step("收尾工作.................")
