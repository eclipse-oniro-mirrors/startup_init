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


class SubStartupToyboxTouchtest0800(TestCase):

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
        result = self.driver.shell(f"help strings")
        self.driver.Assert.contains(result, "usage: strings [-fo] [-t oxd] [-n LEN] [FILE...]")
        self.driver.Assert.contains(result, "Display printable strings in a binary file")
        self.driver.Assert.contains(result, "-f	Show filename")
        self.driver.Assert.contains(result, "-n	At least LEN characters form a string (default 4)")
        self.driver.Assert.contains(result, "-o	Show offset (ala -t d)")
        self.driver.Assert.contains(result, "-t	Show offset type (o=octal, d=decimal, x=hexadecimal)")

        result = self.driver.shell(f"strings --help")
        self.driver.Assert.contains(result, "usage: strings [-fo] [-t oxd] [-n LEN] [FILE...]")
        self.driver.Assert.contains(result, "Display printable strings in a binary file")
        self.driver.Assert.contains(result, "-f	Show filename")
        self.driver.Assert.contains(result, "-n	At least LEN characters form a string (default 4)")
        self.driver.Assert.contains(result, "-o	Show offset (ala -t d)")
        self.driver.Assert.contains(result, "-t	Show offset type (o=octal, d=decimal, x=hexadecimal)")

        # 创建测试文件,文件内容Hello, World!\nThis is a test
        Common.writeDateToFile("test/binaryfile.bin", "\x48\x65\x6c\x6c\x6f\x2c\x20\x57\x6f\x72\x6c\x64\x21\n"
                                                      "\x54\x68\x69\x73\x20\x69\x73\x20\x61\x20\x74\x65\x73\x74")
        self.driver.push_file(f"{os.getcwd()}/test", self.usr_workspace)

        Step("查看二进制文件的字符串")
        result = self.driver.shell(f"strings {self.usr_workspace}/test/binaryfile.bin")
        self.driver.Assert.contains(result, "Hello, World!\nThis is a test")

        Step("只显示字符串长度大于等于14的字符串")
        result = self.driver.shell(f"strings -n 14 {self.usr_workspace}/test/binaryfile.bin")
        self.driver.Assert.contains(result, "This is a test")
        self.driver.Assert.is_true("Hello, World!" not in result)

        Step("查看带有文件名的字符串")
        result = self.driver.shell(f"strings -f {self.usr_workspace}/test/binaryfile.bin")
        self.driver.Assert.contains(result, "binaryfile.bin: Hello, World!")
        self.driver.Assert.contains(result, "binaryfile.bin: This is a test")

        Step("字符串显示十六进制的偏移量")
        result = self.driver.shell(f"strings -t x {self.usr_workspace}/test/binaryfile.bin")
        self.driver.Assert.contains(result, "0 Hello, World!")
        self.driver.Assert.contains(result, "f This is a test")

        Step("字符串显示十进制的偏移量")
        result = self.driver.shell(f"strings -t d {self.usr_workspace}/test/binaryfile.bin")
        self.driver.Assert.contains(result, "0 Hello, World!")
        self.driver.Assert.contains(result, "15 This is a test")

        Step("使用-o选项显示偏移量")
        result = self.driver.shell(f"strings -o {self.usr_workspace}/test/binaryfile.bin")
        self.driver.Assert.contains(result, "0 Hello, World!")
        self.driver.Assert.contains(result, "15 This is a test")

        Step("组合测试：显示至少8个字符长的字符串，并且带有文件名和八进制格式的偏移量")
        result = self.driver.shell(f"strings -fo -t o -n 8 {self.usr_workspace}/test/binaryfile.bin")
        self.driver.Assert.contains(result, "binaryfile.bin:       0 Hello, World!")
        self.driver.Assert.contains(result, "binaryfile.bin:      17 This is a test")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test/")
        shutil.rmtree("test")
        Step("收尾工作")

