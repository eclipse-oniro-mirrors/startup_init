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


class SubStartupToyboxTouchtest0400(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        self.tests = [
            "test_step1",
            "test_step2",
            "test_step3"
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
        result = self.driver.shell(f"help sed")
        self.driver.Assert.contains(result, "sed [-inrzE] [-e SCRIPT]...|SCRIPT [-f SCRIPT_FILE]... [FILE...]")
        self.driver.Assert.contains(result, "Stream editor. Apply one or more editing SCRIPTs to each line of input")
        self.driver.Assert.contains(result, "(from FILE or stdin) producing output (by default to stdout).")
        self.driver.Assert.contains(result, "-e	Add SCRIPT to list")
        self.driver.Assert.contains(result, "-f	Add contents of SCRIPT_FILE to list")
        self.driver.Assert.contains(result,
                                    "-i	Edit each file in place (-iEXT keeps backup file with extension EXT)")
        self.driver.Assert.contains(result, "-n	No default output (use the p command to output matched lines)")
        self.driver.Assert.contains(result, "-r	Use extended regular expression syntax")
        self.driver.Assert.contains(result, "-E	POSIX alias for -r")
        self.driver.Assert.contains(result, "-s	Treat input files separately (implied by -i)")

        result = self.driver.shell(f"sed --help")
        self.driver.Assert.contains(result, "sed [-inrzE] [-e SCRIPT]...|SCRIPT [-f SCRIPT_FILE]... [FILE...]")
        self.driver.Assert.contains(result, "Stream editor. Apply one or more editing SCRIPTs to each line of input")
        self.driver.Assert.contains(result, "(from FILE or stdin) producing output (by default to stdout).")
        self.driver.Assert.contains(result, "-e	Add SCRIPT to list")
        self.driver.Assert.contains(result, "-f	Add contents of SCRIPT_FILE to list")
        self.driver.Assert.contains(result,
                                    "-i	Edit each file in place (-iEXT keeps backup file with extension EXT)")
        self.driver.Assert.contains(result, "-n	No default output (use the p command to output matched lines)")
        self.driver.Assert.contains(result, "-r	Use extended regular expression syntax")
        self.driver.Assert.contains(result, "-E	POSIX alias for -r")
        self.driver.Assert.contains(result, "-s	Treat input files separately (implied by -i)")

        # 创建测试文件
        data = """apple apple is a fruit.
banana apple is a fruit too.
happy old year
I like to eat apples and bananas.
This is a test file for sed.
The old man sat on the park bench, watching the children play.
fruit apple
fruit banana
fruit orange"""

        Common.writeDateToFile("test/example.txt", data)
        self.driver.push_file(f"{os.getcwd()}/test", self.usr_workspace)

    def test_step2(self):
        Step("将文件中的 apple 替换为 orange")
        result = self.driver.shell(f"sed 's/apple/orange/' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "orange apple is a fruit.")
        self.driver.Assert.contains(result, "banana orange is a fruit too.")
        self.driver.Assert.contains(result, "fruit orange")

        Step("将每行中所有的 apple 替换为 orange")
        result = self.driver.shell(f"sed 's/apple/orange/g' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "orange orange is a fruit.")

        Step("只在第一行替换 apple 为 orange")
        result = self.driver.shell(f"sed '1 s/apple/orange/' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "orange apple is a fruit.")
        self.driver.Assert.contains(result, "fruit apple")

        Step("使用正则表达式匹配替换")
        result = self.driver.shell(f"sed 's/^fruit\(.*\)$/\\1 is good/' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "orange is good")

        Step("删除包含 test 的行")
        result = self.driver.shell(f"sed '/test/d' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.is_true("test" not in result)

        Step("在第一行后插入")
        result = self.driver.shell(f"sed '1 a\\New line' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "New line")

        Step("在每行末尾追加- End of line")
        result = self.driver.shell(f"sed 's/$/ - End of line/' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "fruit orange - End of line")

        Step("仅打印包含 apple 的行")
        result = self.driver.shell(f"sed -n '/apple/p' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "apple apple is a fruit.")
        self.driver.Assert.contains(result, "banana apple is a fruit too.")
        self.driver.Assert.contains(result, "I like to eat apples and bananas.")
        self.driver.Assert.contains(result, "fruit apple")

    def test_step3(self):
        Step("打印第一行和第三行")
        result = self.driver.shell(f"sed -n '1p; 3p' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "apple apple is a fruit.")
        self.driver.Assert.contains(result, "happy old year")

        Step("将文件中的 old 替换为 new，并在原文件上进行编辑，同时创建备份文件。")
        self.driver.shell(f"sed -i.bak 's/old/new/' {self.usr_workspace}/test/example.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "happy new year")

        Step("编辑文件并显示已更改的内容")
        result = self.driver.shell(f"sed -n 's/new/old/p' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "happy old year")
        self.driver.Assert.contains(result, "The old man sat on the park bench, watching the children play.")

        Step("组合多个 sed 命令，先替换 apple 为 orange，再在每行末尾追加 -fruit")
        result = self.driver.shell(f"sed -e 's/apple/orange/' -e 's/$/ - fruit/' {self.usr_workspace}/test/example.txt")
        self.driver.Assert.contains(result, "orange apple is a fruit.")
        self.driver.Assert.contains(result, " - fruit")

        Step("在每一行前面加上行号")
        result = self.driver.shell(f"sed '=' {self.usr_workspace}/test/example.txt | sed 'N; s/\n/\t/'")
        self.driver.Assert.contains(result, "1")
        self.driver.Assert.contains(result, "2")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test/")
        shutil.rmtree("test")
        Step("收尾工作")

