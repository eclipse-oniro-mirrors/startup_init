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


class SunStartupInitBase2100(TestCase):

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
        Step("mkdir创建多级目录")
        self.driver.shell("mkdir -p /data/test_utils/a/b/c")
        result = self.driver.shell("ls -ld /data/test_utils/a/b/c")
        self.driver.Assert.contains(result, "c")
        self.driver.shell("mkdir /data/test_utils/single_dir")
        result = self.driver.shell("ls -d /data/test_utils/single_dir")
        self.driver.Assert.contains(result, "single_dir")

        Step("touch创建文件和echo写入内容")
        self.driver.shell("echo 'init_utils_test' > /data/test_utils/write_test.txt")
        result = self.driver.shell("cat /data/test_utils/write_test.txt")
        self.driver.Assert.contains(result, "init_utils_test")

        Step("sed替换字符串模拟StringReplaceChr")
        self.driver.shell("echo 'abc' | sed 's/a/d/' > /data/test_utils/str_replace.txt")
        result = self.driver.shell("cat /data/test_utils/str_replace.txt")
        self.driver.Assert.contains(result, "dbc")

        Step("sed前缀剔除模拟TrimHead")
        self.driver.shell("echo '.trim' | sed 's/^\\.//' > /data/test_utils/trim_test.txt")
        result = self.driver.shell("cat /data/test_utils/trim_test.txt")
        self.driver.Assert.equal(result.split()[0], "trim")

    def teardown(self):
        Step("清理测试目录")
        self.driver.shell("rm -rf /data/test_utils")
        Step("收尾工作.................")
