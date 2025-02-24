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
from hypium import UiDriver
from devicetest.core.test_case import Step, TestCase


class SubStartupToyboxTouchtest0500(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        self.tests = [
            "test_step1",
            "test_step2"
        ]
        TestCase.__init__(self, self.TAG, controllers)

    def setup(self):
        Step("预置工作:初始化PC开始")
        Step(self.devices[0].device_id)
        self.driver = UiDriver(self.device1)

    def test_step1(self):
        Step("显示帮助命令")
        result = self.driver.shell(f"help seq")
        self.driver.Assert.contains(result, "usage: seq [-w|-f fmt_str] [-s sep_str] [first] [increment] last")
        self.driver.Assert.contains(result, "Count from first to last, by increment. Omitted arguments default")
        self.driver.Assert.contains(result, "to 1. Two arguments are used as first and last. Arguments can be")
        self.driver.Assert.contains(result, "negative or floating point.")
        self.driver.Assert.contains(result, "-f	Use fmt_str as a printf-style floating point format string")
        self.driver.Assert.contains(result, "-s	Use sep_str as separator, default is a newline character")
        self.driver.Assert.contains(result, "-w	Pad to equal width with leading zeroes")

        result = self.driver.shell(f"seq --help")
        self.driver.Assert.contains(result, "usage: seq [-w|-f fmt_str] [-s sep_str] [first] [increment] last")
        self.driver.Assert.contains(result, "Count from first to last, by increment. Omitted arguments default")
        self.driver.Assert.contains(result, "to 1. Two arguments are used as first and last. Arguments can be")
        self.driver.Assert.contains(result, "negative or floating point.")
        self.driver.Assert.contains(result, "-f	Use fmt_str as a printf-style floating point format string")
        self.driver.Assert.contains(result, "-s	Use sep_str as separator, default is a newline character")
        self.driver.Assert.contains(result, "-w	Pad to equal width with leading zeroes")

        Step("生成默认序列")
        result = self.driver.shell(f"seq 1 5")
        self.driver.Assert.contains(result, "1\n2\n3\n4\n5")

        Step("指定起始值、增量和结束值")
        result = self.driver.shell(f"seq 2 2 10")
        self.driver.Assert.contains(result, "2\n4\n6\n8\n10\n")

        Step("使用 -w 选项补零")
        result = self.driver.shell(f"seq -w 1 2 10")
        self.driver.Assert.contains(result, "01\n03\n05\n07\n09\n")

        Step("使用 -f 选项指定格式")
        result = self.driver.shell(f"seq -f \"%.2f\" 0.1 0.2 0.5")
        self.driver.Assert.contains(result, "0.10\n0.30\n0.50")

        Step("使用 -w 选项补零")
        result = self.driver.shell(f"seq -w 1 2 10")
        self.driver.Assert.contains(result, "01\n03\n05\n07\n09\n")

        Step("自定义分隔符")
        result = self.driver.shell(f"seq -s \",\" 1 5")
        self.driver.Assert.contains(result, "1,2,3,4,5")

        Step("生成倒序序列")
        result = self.driver.shell(f"seq 10 -2 2")
        self.driver.Assert.contains(result, "10\n8\n6\n4\n2")

        Step("生成空序列")
        result = self.driver.shell(f"seq 5 1 1")
        self.driver.Assert.is_true(result == "")

        Step("-f与-w选项同时出现")
        result = self.driver.shell(f"seq -w -s \",\" -f \"%.2f\" 0.1 0.2 0.5")
        self.driver.Assert.contains(result, "No 'f' with 'w' (see \"seq --help\")")

        Step("使用 -w 选项补零并使用“，”分割符")
        result = self.driver.shell(f"seq -w -s \",\" 1 2 10")
        self.driver.Assert.contains(result, "01,03,05,07")

    def test_step2(self):
        Step("新增测试点")
        result = self.driver.shell(f"seq -f '|%8.1f|' 99.5 0.3 100.5")
        self.driver.Assert.contains(result, "|    99.5|\n|    99.8|\n|   100.1|\n|   100.4|")
        result = self.driver.shell(f"seq -f '|%08.1f|' 99.5 0.3 100.5")
        self.driver.Assert.contains(result, "|000099.5|\n|000099.8|\n|000100.1|\n|000100.4|")
        result = self.driver.shell(f"seq -f '|%-8.1f|' 99.5 0.3 100.5")
        self.driver.Assert.contains(result, "|99.5    |\n|99.8    |\n|100.1   |\n|100.4   |")
        result = self.driver.shell(f"seq 10 2")
        self.driver.Assert.equal(result, "")
        result = self.driver.shell(f"seq 10 0 1")
        self.driver.Assert.equal(result, "")

    def teardown(self):
        Step("收尾工作")

