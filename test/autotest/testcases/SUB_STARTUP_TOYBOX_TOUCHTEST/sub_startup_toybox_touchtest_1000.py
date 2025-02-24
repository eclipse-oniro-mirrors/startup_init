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


class SubStartupToyboxTouchtest1000(TestCase):

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
        result = self.driver.shell(f"help tail")
        self.driver.Assert.contains(result, "usage: tail [-n|c NUMBER] [-f] [FILE...]")
        self.driver.Assert.contains(result, "Copy last lines from files to stdout. If no files listed, copy from")
        self.driver.Assert.contains(result, "stdin. Filename \"-\" is a synonym for stdin.")
        self.driver.Assert.contains(result, "-n	Output the last NUMBER lines (default 10), +X counts from start")
        self.driver.Assert.contains(result, "-c	Output the last NUMBER bytes, +NUMBER counts from start")
        self.driver.Assert.contains(result, "-f	Follow FILE(s), waiting for more data to be appended")

        result = self.driver.shell(f"tail --help")
        self.driver.Assert.contains(result, "usage: tail [-n|c NUMBER] [-f] [FILE...]")
        self.driver.Assert.contains(result, "Copy last lines from files to stdout. If no files listed, copy from")
        self.driver.Assert.contains(result, "stdin. Filename \"-\" is a synonym for stdin.")
        self.driver.Assert.contains(result, "-n	Output the last NUMBER lines (default 10), +X counts from start")
        self.driver.Assert.contains(result, "-c	Output the last NUMBER bytes, +NUMBER counts from start")
        self.driver.Assert.contains(result, "-f	Follow FILE(s), waiting for more data to be appended")

        # 创建测试文件
        Common.writeDateToFile("test/file.txt", "line 1\nline 2\nline 3\nline 4\nline 5\nline 6\nline 7\n"
                                                    "line 8\nline 9\nline 10\nline 11\nline 12\nline 13\nline 14\n"
                                                    "line 15\nline 16\nline 17\nline 18\nline 19\nline 20")
        self.driver.push_file(f"{os.getcwd()}/test", self.usr_workspace)

        Step("显示默认的最后十行")
        result = self.driver.shell(f"tail {self.usr_workspace}/test/file.txt")
        self.driver.Assert.is_true("line 1\r\n" not in result)
        self.driver.Assert.is_true("line 10\r\n" not in result)
        self.driver.Assert.contains(result, "line 11")
        self.driver.Assert.contains(result, "line 20")

        Step("从第四行开始显示到末尾")
        result = self.driver.shell(f"tail -n +4 {self.usr_workspace}/test/file.txt")
        self.driver.Assert.is_true("line 1\r\n" not in result)
        self.driver.Assert.is_true("line 3\r\n" not in result)
        self.driver.Assert.contains(result, "line 4")
        self.driver.Assert.contains(result, "line 20")

        Step("显示指定字节数，显示最后10个字节")
        result = self.driver.shell(f"tail -c 10 {self.usr_workspace}/test/file.txt")
        self.driver.Assert.is_true("line 1\r\n" not in result)
        self.driver.Assert.contains(result, "9")
        self.driver.Assert.contains(result, "line 20")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test/")
        shutil.rmtree("test")
        Step("收尾工作")

