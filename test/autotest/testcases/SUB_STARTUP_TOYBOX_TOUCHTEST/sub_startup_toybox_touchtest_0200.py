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


class SubStartupToyboxTouchtest0200(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        self.tests = [
            "test_step1",
            "test_step2"
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
        result = self.driver.shell(f"help rm")
        self.driver.Assert.contains(result, "usage: rm [-fiRrv] FILE...")
        self.driver.Assert.contains(result, "Remove each argument from the filesystem.")
        self.driver.Assert.contains(result, "-f	Force: remove without confirmation, no error if it doesn't exist")
        self.driver.Assert.contains(result, "-i	Interactive: prompt for confirmation")
        self.driver.Assert.contains(result, "-rR	Recursive: remove directory contents")
        self.driver.Assert.contains(result, "-v	Verbose")

        result = self.driver.shell(f"rm --help")
        self.driver.Assert.contains(result, "usage: rm [-fiRrv] FILE...")
        self.driver.Assert.contains(result, "Remove each argument from the filesystem.")
        self.driver.Assert.contains(result, "-f	Force: remove without confirmation, no error if it doesn't exist")
        self.driver.Assert.contains(result, "-i	Interactive: prompt for confirmation")
        self.driver.Assert.contains(result, "-rR	Recursive: remove directory contents")
        self.driver.Assert.contains(result, "-v	Verbose")

        self.driver.shell(f"mkdir {self.usr_workspace}/test/")
        # 创建多个文件
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test1.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test2.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test3.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test4.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test5.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test6.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test7.txt")

        Step("删除单个文件")
        self.driver.shell(f"rm {self.usr_workspace}/test/test.txt")
        result = self.driver.shell(f"ls {self.usr_workspace}/test/")
        self.driver.Assert.is_true("test.txt" not in result)

        Step("删除不存在的文件")
        result = self.driver.shell(f"rm {self.usr_workspace}/test/noexittest.txt")
        self.driver.Assert.contains(result, "No such file or directory")

        Step("强制删除文件，不进行确认")
        self.driver.shell(f"rm -f {self.usr_workspace}/test/test2.txt")
        result = self.driver.shell(f"ls {self.usr_workspace}/test/")
        self.driver.Assert.is_true("test2.txt" not in result)

        Step("递归删除目录所有内容")
        self.driver.shell(f"rm -r {self.usr_workspace}/test")
        result = self.driver.shell(f"ls {self.usr_workspace}/")
        self.driver.Assert.is_true("test" not in result)

        Step("详细模式下删除目录下所有文件")
        self.driver.shell(f"mkdir {self.usr_workspace}/test/")
        # 创建多个文件
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test1.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test2.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test3.txt")

        result = self.driver.shell(f"rm -v {self.usr_workspace}/test/*")
        self.driver.Assert.contains(result, f"rm '{self.usr_workspace}/test/test1.txt'")

    def test_step2(self):
        Step("强制、递归、详细模式下删除目录")
        self.driver.shell(f"mkdir {self.usr_workspace}/test/")
        # 创建多个文件
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test1.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test2.txt")
        self.driver.shell(f"echo test > {self.usr_workspace}/test/test3.txt")

        result = self.driver.shell(f"rm -vfR {self.usr_workspace}/test")
        self.driver.Assert.contains(result, f"rm '{self.usr_workspace}/test/test3.txt'")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test/")
        shutil.rmtree("test")
        Step("收尾工作")

