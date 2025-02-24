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


class SubStartupToyboxTouchtest0300(TestCase):

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

    def test_step1(self):
        Step("显示帮助命令")
        result = self.driver.shell(f"help rmdir")
        self.driver.Assert.contains(result, "usage: rmdir [-p] [dirname...]")
        self.driver.Assert.contains(result, "Remove one or more directories.")
        self.driver.Assert.contains(result, "-p	Remove path")
        self.driver.Assert.contains(result,
                                    "--ignore-fail-on-non-empty	Ignore failures caused by non-empty directories")

        result = self.driver.shell(f"rmdir --help")
        self.driver.Assert.contains(result, "usage: rmdir [-p] [dirname...]")
        self.driver.Assert.contains(result, "Remove one or more directories.")
        self.driver.Assert.contains(result, "-p	Remove path")
        self.driver.Assert.contains(result,
                                    "--ignore-fail-on-non-empty	Ignore failures caused by non-empty directories")

        Step("删除一个空目录")
        self.driver.shell(f"mkdir {self.usr_workspace}/test2")
        self.driver.shell(f"rmdir {self.usr_workspace}/test2")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/")
        self.driver.Assert.is_true("test2" not in result)

        Step("删除多个目录")
        self.driver.shell(f"mkdir {self.usr_workspace}/test2")
        self.driver.shell(f"mkdir {self.usr_workspace}/test")
        self.driver.shell(f"rmdir {self.usr_workspace}/test2 {self.usr_workspace}/test")
        result = self.driver.shell(f"ls -l {self.usr_workspace}")
        self.driver.Assert.is_true("test" not in result)

        Step("删除目录路径")
        self.driver.shell(f"mkdir {self.usr_workspace}/test")
        self.driver.shell(f"mkdir {self.usr_workspace}/test/test2/")
        self.driver.shell(f"mkdir {self.usr_workspace}/test/test2/test3")
        self.driver.shell(f"rmdir -p {self.usr_workspace}/test/test2/test3")
        result = self.driver.shell(f"ls -l {self.usr_workspace}")
        self.driver.Assert.is_true("test" not in result)

        Step("删除目录路径,其中一个目录非空")
        self.driver.shell(f"mkdir {self.usr_workspace}/test")
        self.driver.shell(f"mkdir {self.usr_workspace}/test/test2/")
        self.driver.shell(f"mkdir {self.usr_workspace}/test/test2/test3")
        self.driver.shell(f"echo hello > {self.usr_workspace}/test/test.txt")
        result = self.driver.shell(f"rmdir -p {self.usr_workspace}/test/test2/test3")
        self.driver.Assert.contains(result, "Directory not empty")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.is_true("test2" not in result)

        Step("忽略非空目录的删除失败")
        self.driver.shell(f"mkdir {self.usr_workspace}/test/test2/")
        self.driver.shell(f"mkdir {self.usr_workspace}/test/test2/test3")
        self.driver.shell(f"echo hello > {self.usr_workspace}/test/test.txt")
        result = self.driver.shell(f"rmdir -p --ignore-fail-on-non-empty {self.usr_workspace}/test/test2/test3")
        self.driver.Assert.is_true("Directory not empty" not in result)

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test/")
        Step("收尾工作")

