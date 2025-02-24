#
# Copyright (c) 2025 Huawei Device Co., Ltd.
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

# -*- coding: utf-8 -*-
import shutil
import os
from hypium import UiDriver
from devicetest.core.test_case import Step, TestCase
from aw import Common


class SubStartupToyboxTouchtest0900(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.usr_workspace = "/data/local/tmp"

    def setup(self):
        Step("预置工作:初始化PC开始")
        Step(self.devices[0].device_id)
        self.driver = UiDriver(self.device1)
        if os.path.exists("test") != True:
            os.mkdir("test")

    def test_step1(self):
        Step("显示帮助命令")
        result = self.driver.shell(f"help tac")
        self.driver.Assert.contains(result, "usage: tac [FILE...]")
        self.driver.Assert.contains(result, "Output lines in reverse order.")

        result = self.driver.shell(f"tac --help")
        self.driver.Assert.contains(result, "usage: tac [FILE...]")
        self.driver.Assert.contains(result, "Output lines in reverse order.")

        # 创建测试文件
        data = ""
        for i in range(10):
            data = data + str(2 * i + 1) + "\n"

        Common.writeDateToFile("test/test.txt", data)
        Common.writeDateToFile("test/test2.txt", "line3\nline2\nline1\nline4\nline33\n")
        self.driver.push_file(f"{os.getcwd()}/test", self.usr_workspace)

        Step("输出文件中的所有行，以相反的顺序显示")
        result = self.driver.shell(f"tac  {self.usr_workspace}/test/test.txt")
        self.driver.Assert.contains(result, "19\r\n17\r\n")

        Step("处理多个文件")
        result = self.driver.shell(f"tac {self.usr_workspace}/test/test.txt {self.usr_workspace}/test/test2.txt")
        self.driver.Assert.contains(result, "19\r\n17\r\n")
        self.driver.Assert.contains(result, "line33\r\nline4\r\n")

        Step("从标准输入读取")
        result = self.driver.shell(f"echo -e \"line1\nline2\nline3\" | tac")
        self.driver.Assert.contains(result, "line3\nline2\nline1")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test/")
        shutil.rmtree("test")
        Step("收尾工作")
