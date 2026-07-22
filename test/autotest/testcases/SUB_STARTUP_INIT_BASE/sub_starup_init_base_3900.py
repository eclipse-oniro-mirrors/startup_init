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


class SunStartupInitBase3900(TestCase):
    """验证init基础命令(cmds)功能:
    - mkdir/chown/chmod/write/rm等文件操作命令
    - export环境变量设置
    - 对应unittest: cmds_unittest.cpp
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
        Step("测试mkdir命令-创建测试目录")
        self.driver.shell("mkdir -p /data/test_cmds_dir")
        result = self.driver.shell("ls -d /data/test_cmds_dir")
        self.driver.Assert.contains(result, "test_cmds_dir")

        Step("测试write命令-写入测试文件")
        self.driver.shell("echo 'test_content' > /data/test_cmds_dir/test_file.txt")
        result = self.driver.shell("cat /data/test_cmds_dir/test_file.txt")
        self.driver.Assert.contains(result, "test_content")

        Step("测试chmod命令-修改文件权限")
        self.driver.shell("chmod 777 /data/test_cmds_dir/test_file.txt")
        result = self.driver.shell("ls -l /data/test_cmds_dir/test_file.txt")
        self.driver.Assert.contains(result, "rwxrwxrwx")

        Step("测试chown命令-修改文件属主")
        self.driver.shell("chown 1000:1000 /data/test_cmds_dir/test_file.txt")
        result = self.driver.shell("ls -l /data/test_cmds_dir/test_file.txt")
        Step("chown结果: %s" % result)

        Step("测试rm命令-删除文件")
        self.driver.shell("rm -f /data/test_cmds_dir/test_file.txt")
        result = self.driver.shell("ls /data/test_cmds_dir/test_file.txt 2>&1")
        self.driver.Assert.contains(result, "No such file")

        Step("测试rmdir命令-删除目录")
        self.driver.shell("rmdir /data/test_cmds_dir")
        result = self.driver.shell("ls -d /data/test_cmds_dir 2>&1")
        self.driver.Assert.contains(result, "No such file")

    def teardown(self):
        Step("收尾工作.................")
        self.driver.shell("rm -rf /data/test_cmds_dir")
