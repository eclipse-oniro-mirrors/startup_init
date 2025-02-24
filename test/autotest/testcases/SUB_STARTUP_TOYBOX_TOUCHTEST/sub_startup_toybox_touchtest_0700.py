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


class SubStartupToyboxTouchtest0700(TestCase):

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
        result = self.driver.shell(f"help split")
        self.driver.Assert.contains(result, "usage: split [-a SUFFIX_LEN] [-b BYTES] [-l LINES] [INPUT [OUTPUT]]")
        self.driver.Assert.contains(result, "-a	Suffix length (default 2)")
        self.driver.Assert.contains(result, "-b	BYTES/file (10, 10k, 10m, 10g...)")
        self.driver.Assert.contains(result, "-l	LINES/file (default 1000)")

        result = self.driver.shell(f"split --help")
        self.driver.Assert.contains(result, "usage: split [-a SUFFIX_LEN] [-b BYTES] [-l LINES] [INPUT [OUTPUT]]")
        self.driver.Assert.contains(result, "-a	Suffix length (default 2)")
        self.driver.Assert.contains(result, "-b	BYTES/file (10, 10k, 10m, 10g...)")
        self.driver.Assert.contains(result, "-l	LINES/file (default 1000)")

        # 创建测试文件
        data = ""
        for i in range(100):
            data = data + str(i) + "\n"

        Common.writeDateToFile("test/numbers.txt", data)
        Common.generateLargeFile("test/large_file.bin", 10)
        self.driver.push_file(f"{os.getcwd()}/test", self.usr_workspace)

        Step("使用 split 命令按照每个文件 10 行的行数分割")
        self.driver.shell(f"split -l 10 {self.usr_workspace}/test/numbers.txt "
                                   f"{self.usr_workspace}/test/output_")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.contains(result, "output_aa")
        self.driver.Assert.contains(result, "output_ab")
        self.driver.Assert.contains(result, "output_aj")

        Step("使用 split 命令按照每个文件大小为 100KB 分割")
        self.driver.shell(f"split -b 100k {self.usr_workspace}/test/large_file.bin {self.usr_workspace}/test/outputlarge_")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.contains(result, str(100 * 1024))
        self.driver.Assert.contains(result, "outputlarge_")

        Step("使用 split 命令按照每个文件 10 行的行数分割 numbers.txt 文件，并指定后缀长度为 3")
        self.driver.shell(f"split -a 3 -l 10 {self.usr_workspace}/test/numbers.txt {self.usr_workspace}/test/output3_")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.contains(result, "output3_aaa")
        self.driver.Assert.contains(result, "output3_aaj")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test/")
        shutil.rmtree("test")
        Step("收尾工作")

