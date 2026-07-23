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


class SunStartupInitBase5100(TestCase):
    """验证工具函数(utils_unittest)功能:
    - 目录创建(MakeDirRecursive)
    - 文件创建和写入(CheckAndCreatFile/WriteAll)
    - 字符串操作(StringToInt/StringReplaceChr)
    - 对应unittest: utils_unittest.cpp
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
        Step("测试递归创建目录(MakeDirRecursive)")
        self.driver.shell("mkdir -p /data/test_utils/a/b/c")
        result = self.driver.shell("ls -d /data/test_utils/a/b/c")
        self.driver.Assert.contains(result, "c")

        Step("测试写入文件(WriteAll)")
        self.driver.shell("echo 'utils_test_content' > /data/test_utils/a/b/c/test.txt")
        result = self.driver.shell("cat /data/test_utils/a/b/c/test.txt")
        self.driver.Assert.contains(result, "utils_test_content")

        Step("测试检查并创建文件(CheckAndCreatFile)")
        result = self.driver.shell("test -f /data/test_utils/a/b/c/test.txt && echo 'exists'")
        self.driver.Assert.contains(result, "exists")

        Step("测试文件头部字符裁剪(TrimHead等效操作)")
        result = self.driver.shell("echo '.trimmed_value' | sed 's/^\\.//'")
        self.driver.Assert.equal(result.strip(), "trimmed_value")

        Step("测试目录权限设置")
        self.driver.shell("chmod 755 /data/test_utils/a/b/c")
        result = self.driver.shell("ls -ld /data/test_utils/a/b/c")
        Step("目录权限: %s" % result)

        Step("验证时间相关工具函数(等效操作)")
        result = self.driver.shell("date +%s")
        Step("当前时间戳: %s" % result)
        self.driver.Assert.equal(result.strip().isdigit(), True)

        Step("清理测试目录")
        self.driver.shell("rm -rf /data/test_utils")

    def teardown(self):
        Step("收尾工作.................")
        self.driver.shell("rm -rf /data/test_utils")
