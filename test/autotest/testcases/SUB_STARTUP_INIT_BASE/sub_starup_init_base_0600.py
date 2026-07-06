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


class SunStartupInitBase0600(TestCase):

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
        self.driver.shell("mkdir -p /data/test_cmds_dir/test_dir0")
        result = self.driver.shell("ls -d /data/test_cmds_dir/test_dir0")
        self.driver.Assert.contains(result, "test_dir0")

        Step("chmod修改目录权限为777")
        self.driver.shell("chmod 777 /data/test_cmds_dir/test_dir0")
        result = self.driver.shell("ls -ld /data/test_cmds_dir/test_dir0")
        self.driver.Assert.contains(result, "drwxrwxrwx")

        Step("echo写入文件并cat读取验证")
        self.driver.shell("echo 'init_cmd_test_data' > /data/test_cmds_dir/test_src.txt")
        result = self.driver.shell("cat /data/test_cmds_dir/test_src.txt")
        self.driver.Assert.contains(result, "init_cmd_test_data")

        Step("cp拷贝文件并验证")
        self.driver.shell("cp /data/test_cmds_dir/test_src.txt /data/test_cmds_dir/test_dst.txt")
        result = self.driver.shell("ls /data/test_cmds_dir/test_dst.txt")
        self.driver.Assert.contains(result, "test_dst.txt")

        Step("rm删除文件和rmdir删除目录")
        self.driver.shell("rm /data/test_cmds_dir/test_dst.txt")
        result = self.driver.shell("ls /data/test_cmds_dir/")
        self.driver.Assert.equal('test_dst.txt' not in result, True)
        self.driver.shell("rmdir /data/test_cmds_dir/test_dir0")
        result = self.driver.shell("ls /data/test_cmds_dir/")
        self.driver.Assert.equal('test_dir0' not in result, True)

    def teardown(self):
        Step("清理测试目录")
        self.driver.shell("rm -rf /data/test_cmds_dir")
        Step("收尾工作.................")
