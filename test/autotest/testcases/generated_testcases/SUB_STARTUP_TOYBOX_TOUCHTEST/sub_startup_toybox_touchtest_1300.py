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


class SubStartupToyboxTouchtest1300(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        self.tests = [
            "test_step1",
            "test_step2",
            "test_step3",
            "test_step4",
            "test_step5"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.usr_workspace = "/data/local/tmp"

    def setup(self):
        Step("预置工作:初始化PC开始")
        Step(self.devices[0].device_id)
        self.driver = UiDriver(self.device1)
        if os.path.exists("test_mv") != True:
            os.mkdir("test_mv")

    def test_step1(self):
        Step("显示mv帮助命令")
        result = self.driver.shell(f"help mv")
        self.driver.Assert.contains(result, "mv [-fHiLn] SOURCE... DEST")
        self.driver.Assert.contains(result, "-f	Force copy by deleting destination file")
        self.driver.Assert.contains(result, "-H	Follow symlinks listed on command line")
        self.driver.Assert.contains(result, "-i	Interactive, prompt before overwrite")
        self.driver.Assert.contains(result, "-L	Follow all symlinks")
        self.driver.Assert.contains(result, "-n	No clobber (don't overwrite DEST)")

        result = self.driver.shell(f"mv --help")
        self.driver.Assert.contains(result, "mv [-fHiLn] SOURCE... DEST")
        self.driver.Assert.contains(result, "-f	Force copy by deleting destination file")

    def test_step2(self):
        Common.writeDateToFile("test_mv/source.txt", "Hello, mv test!")
        Common.writeDateToFile("test_mv/source2.txt", "Second file content")
        self.driver.push_file(f"{os.getcwd()}/test_mv", self.usr_workspace)

        Step("移动单个文件")
        self.driver.shell(f"mv {self.usr_workspace}/test_mv/source.txt {self.usr_workspace}/test_mv/moved.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test_mv/moved.txt")
        self.driver.Assert.contains(result, "Hello, mv test!")

        Step("验证源文件已不存在")
        result = self.driver.shell(f"ls {self.usr_workspace}/test_mv/source.txt 2>&1")
        self.driver.Assert.contains(result, "No such file or directory")

        Step("不覆盖目标文件")
        self.driver.shell(f"touch {self.usr_workspace}/test_mv/existing.txt")
        self.driver.shell(f"echo existing_content > {self.usr_workspace}/test_mv/existing.txt")
        self.driver.shell(f"mv -n {self.usr_workspace}/test_mv/source2.txt {self.usr_workspace}/test_mv/existing.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test_mv/existing.txt")
        self.driver.Assert.contains(result, "existing_content")

    def test_step3(self):
        Step("创建符号链接并移动")
        self.driver.shell(f"ln -s {self.usr_workspace}/test_mv/moved.txt {self.usr_workspace}/test_mv/moved_link.txt")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test_mv/moved_link.txt")
        self.driver.Assert.contains(result, "moved_link.txt")

        Step("移动符号链接本身")
        self.driver.shell(f"mv {self.usr_workspace}/test_mv/moved_link.txt {self.usr_workspace}/test_mv/moved_link_moved.txt")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test_mv/")
        self.driver.Assert.contains(result, "moved_link_moved.txt")

        Step("移动目录")
        self.driver.shell(f"mkdir {self.usr_workspace}/test_mv/subdir")
        self.driver.shell(f"touch {self.usr_workspace}/test_mv/subdir/afile.txt")
        self.driver.shell(f"mv {self.usr_workspace}/test_mv/subdir {self.usr_workspace}/test_mv/subdir_moved")
        result = self.driver.shell(f"ls {self.usr_workspace}/test_mv/subdir_moved")
        self.driver.Assert.contains(result, "afile.txt")

    def test_step4(self):
        Step("移动文件到目录")
        self.driver.shell(f"touch {self.usr_workspace}/test_mv/target_dir_file.txt")
        self.driver.shell(f"mkdir {self.usr_workspace}/test_mv/target_dir")
        self.driver.shell(f"mv {self.usr_workspace}/test_mv/target_dir_file.txt {self.usr_workspace}/test_mv/target_dir/")
        result = self.driver.shell(f"ls {self.usr_workspace}/test_mv/target_dir")
        self.driver.Assert.contains(result, "target_dir_file.txt")

        Step("强制覆盖文件")
        self.driver.shell(f"echo first > {self.usr_workspace}/test_mv/overwrite.txt")
        self.driver.shell(f"echo second > {self.usr_workspace}/test_mv/overwrite2.txt")
        self.driver.shell(f"mv -f {self.usr_workspace}/test_mv/overwrite2.txt {self.usr_workspace}/test_mv/overwrite.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test_mv/overwrite.txt")
        self.driver.Assert.contains(result, "second")

    def test_step5(self):
        Step("移动多个文件到目录")
        self.driver.shell(f"touch {self.usr_workspace}/test_mv/multi1.txt")
        self.driver.shell(f"touch {self.usr_workspace}/test_mv/multi2.txt")
        self.driver.shell(f"touch {self.usr_workspace}/test_mv/multi3.txt")
        self.driver.shell(f"mkdir {self.usr_workspace}/test_mv/multi_target")
        self.driver.shell(f"mv {self.usr_workspace}/test_mv/multi1.txt {self.usr_workspace}/test_mv/multi2.txt {self.usr_workspace}/test_mv/multi3.txt {self.usr_workspace}/test_mv/multi_target/")
        result = self.driver.shell(f"ls {self.usr_workspace}/test_mv/multi_target")
        self.driver.Assert.contains(result, "multi1.txt")
        self.driver.Assert.contains(result, "multi2.txt")
        self.driver.Assert.contains(result, "multi3.txt")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test_mv/")
        shutil.rmtree("test_mv")
        Step("收尾工作")
