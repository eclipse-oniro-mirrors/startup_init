#
# Copyright (c) 2026 Huawei Device Co., Ltd.
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
import time
from hypium import UiDriver
from devicetest.core.test_case import Step, TestCase
from aw import Common


class SubStartupToyboxTouchtest1400(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        self.tests = [
            "test_step1",
            "test_step2",
            "test_step3",
            "test_step4"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.usr_workspace = "/data/local/tmp"

    def setup(self):
        Step("预置工作:初始化PC开始")
        Step(self.devices[0].device_id)
        self.driver = UiDriver(self.device1)
        if os.path.exists("test_rm") != True:
            os.mkdir("test_rm")

    def test_step1(self):
        Step("显示rm帮助命令")
        result = self.driver.shell(f"help rm")
        self.driver.Assert.contains(result, "rm [-fDirR] FILE...")
        self.driver.Assert.contains(result, "-f	Force removal")
        self.driver.Assert.contains(result, "-D	Remove even if file is . and ..")
        self.driver.Assert.contains(result, "-i	Interactive")
        self.driver.Assert.contains(result, "-r	Synonym for -R")
        self.driver.Assert.contains(result, "-R	Recurse into subdirectories")

        result = self.driver.shell(f"rm --help")
        self.driver.Assert.contains(result, "rm [-fDirR] FILE...")
        self.driver.Assert.contains(result, "Remove files")

    def test_step2(self):
        Common.writeDateToFile("test_rm/delete_me.txt", "This file will be deleted")
        Common.writeDateToFile("test_rm/keep_me.txt", "This file stays")
        self.driver.push_file(f"{os.getcwd()}/test_rm", self.usr_workspace)

        Step("删除单个文件")
        self.driver.shell(f"rm {self.usr_workspace}/test_rm/delete_me.txt")
        result = self.driver.shell(f"ls {self.usr_workspace}/test_rm/")
        self.driver.Assert.is_true("delete_me.txt" not in result)
        self.driver.Assert.contains(result, "keep_me.txt")

    def test_step3(self):
        Step("强制删除不可写文件")
        Common.writeDateToFile("test_rm/readonly.txt", "readonly content")
        self.driver.push_file(f"{os.getcwd()}/test_rm", self.usr_workspace)
        self.driver.shell(f"chmod 444 {self.usr_workspace}/test_rm/readonly.txt")
        self.driver.shell(f"rm -f {self.usr_workspace}/test_rm/readonly.txt")
        result = self.driver.shell(f"ls {self.usr_workspace}/test_rm/readonly.txt 2>&1")
        self.driver.Assert.contains(result, "No such file or directory")

        Step("递归删除目录")
        self.driver.shell(f"mkdir -p {self.usr_workspace}/test_rm/nested/dir/deep")
        self.driver.shell(f"touch {self.usr_workspace}/test_rm/nested/dir/deep/afile.txt")
        self.driver.shell(f"rm -rf {self.usr_workspace}/test_rm/nested")
        result = self.driver.shell(f"ls {self.usr_workspace}/test_rm/nested 2>&1")
        self.driver.Assert.contains(result, "No such file or directory")

    def test_step4(self):
        Step("删除空目录")
        self.driver.shell(f"mkdir -p {self.usr_workspace}/test_rm/empty_dir")
        time.sleep(0.5)
        self.driver.shell(f"rm -r {self.usr_workspace}/test_rm/empty_dir")
        result = self.driver.shell(f"ls {self.usr_workspace}/test_rm/empty_dir 2>&1")
        self.driver.Assert.contains(result, "No such file or directory")

        Step("删除多个文件")
        self.driver.shell(f"touch {self.usr_workspace}/test_rm/a.txt {self.usr_workspace}/test_rm/b.txt {self.usr_workspace}/test_rm/c.txt")
        self.driver.shell(f"rm {self.usr_workspace}/test_rm/a.txt {self.usr_workspace}/test_rm/b.txt {self.usr_workspace}/test_rm/c.txt")
        result = self.driver.shell(f"ls {self.usr_workspace}/test_rm/a.txt 2>&1")
        self.driver.Assert.contains(result, "No such file or directory")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test_rm/")
        shutil.rmtree("test_rm")
        Step("收尾工作")
