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


class SubStartupToyboxTouchtest0600(TestCase):

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
        result = self.driver.shell(f"help sort")
        self.driver.Assert.contains(result, "usage: sort [-Mbcdfginrsuz] [FILE...] [-k#[,#[x]] [-t X]] [-o FILE]")

        result = self.driver.shell(f"sort --help")
        self.driver.Assert.contains(result, "usage: sort [-Mbcdfginrsuz] [FILE...] [-k#[,#[x]] [-t X]] [-o FILE]")

        # 创建测试文件
        Common.writeDateToFile("test/file.txt", "apple\norange\nbanana\n")
        Common.writeDateToFile("test/file2.txt", "2\n10\n1\n")
        Common.writeDateToFile("test/file3.txt", "John 25\nAlice 30\nBob 20\n")
        Common.writeDateToFile("test/month.txt", "January\nMarch\nFebruary\nAlice 30\nBob 20\n")
        Common.writeDateToFile("test/file4.txt", "apple\nOrange\nBanana\n")
        Common.writeDateToFile("test/versions.txt", "name-1.2.10\nname-1.10.1\nname-1.2.3\n")
        Common.writeDateToFile("test/data.csv", "John,25\nAlice,30\nBob,20\n")
        Common.writeDateToFile("test/file5.txt", "apple\norange\nbanana\nbanana")
        Common.writeDateToFile("test/file6.txt", "   apple\n  orange\n  banana\n")
        Common.writeDateToFile("test/file7.txt", "apple 10\nBanana 20\n123 orange\n0xA0 Grape\n\n0x3F Pineapple\n")
        Common.writeDateToFile("test/nullterminated.txt", "line1\0line2\0line3\0")
        Common.writeDateToFile("test/backupsort.txt", "1 10\n2 8\n3 7\n")
        Common.writeDateToFile("test/file8.txt", "apple\t10\nBanana\t20\nGrape\t15\nOrange\t30\n")

        self.driver.push_file(f"{os.getcwd()}/test", self.usr_workspace)
        self.driver.shell(f"dos2unix {self.usr_workspace}/test/*")

        Step("基本字母排序")
        result = self.driver.shell(f"sort {self.usr_workspace}/test/file.txt")
        Step(result)
        self.driver.Assert.contains(result, "apple\nbanana\norange")

        Step("按数字排序")
        result = self.driver.shell(f"sort -n {self.usr_workspace}/test/file2.txt")
        self.driver.Assert.contains(result, "1\n2\n10")

        Step("按逆序排序")
        result = self.driver.shell(f"sort -r {self.usr_workspace}/test/file.txt")
        self.driver.Assert.contains(result, "orange\nbanana\napple")

        Step("按字段排序")
        result = self.driver.shell(f"sort -k2,2n {self.usr_workspace}/test/file3.txt")
        self.driver.Assert.contains(result, "Bob 20\nJohn 25\nAlice 30")

        Step("按月份排序")
        result = self.driver.shell(f"sort -M {self.usr_workspace}/test/month.txt")
        self.driver.Assert.contains(result, "Alice 30\nBob 20\nJanuary\nFebruary\nMarch\n")

        Step("忽略大小写排序")
        result = self.driver.shell(f"sort -f {self.usr_workspace}/test/file4.txt")
        self.driver.Assert.contains(result, "apple\nBanana\nOrange\n")

    def test_step2(self):
        Step("检查输入是否排序")
        result = self.driver.shell(f"sort -cn {self.usr_workspace}/test/file2.txt")
        self.driver.Assert.contains(result, "Check line 2")

        Step("对包含版本号的文件进行排序")
        result = self.driver.shell(f"sort -V {self.usr_workspace}/test/versions.txt")
        self.driver.Assert.contains(result, "name-1.2.3\nname-1.2.10\nname-1.10.1\n")

        Step("按字段排序")
        result = self.driver.shell(f"sort -t',' -k2,2n {self.usr_workspace}/test/data.csv")
        self.driver.Assert.contains(result, "Bob,20\nJohn,25\nAlice,30\n")

        Step("仅输出唯一行")
        result = self.driver.shell(f"sort -u {self.usr_workspace}/test/file5.txt")
        self.driver.Assert.contains(result, "apple\nbanana\norange")

        Step("忽略行首空格对文本排序")
        result = self.driver.shell(f"sort -b {self.usr_workspace}/test/file6.txt")
        self.driver.Assert.contains(result, "   apple\n  banana\n  orange\n")

        Step("按照十六进制数值进行排序")
        result = self.driver.shell(f"sort -x {self.usr_workspace}/test/file7.txt")
        self.driver.Assert.contains(result, "0x3F Pineapple\n0xA0 Grape")

        Step("处理以空字符结尾的行")
        result = self.driver.shell(f"sort -z {self.usr_workspace}/test/nullterminated.txt")
        self.driver.Assert.contains(result, "line1")

        Step("跳过后备排序")
        result = self.driver.shell(f"sort -s -k2 -n {self.usr_workspace}/test/backupsort.txt")
        self.driver.Assert.contains(result, "3 7\n2 8\n1 10")

        Step("字典顺序排序")
        result = self.driver.shell(f"sort -d {self.usr_workspace}/test/file.txt")
        self.driver.Assert.contains(result, "apple\nbanana\norange")

        Step("忽略非打印字符排序")
        result = self.driver.shell(f"sort -i {self.usr_workspace}/test/file8.txt")
        self.driver.Assert.contains(result, "Banana	20\nGrape	15\nOrange	30\napple	10")

        Step("将排序结果输出到文件")
        self.driver.shell(f"sort -f {self.usr_workspace}/test/file4.txt -o {self.usr_workspace}/test/sorted_file.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/sorted_file.txt")
        self.driver.Assert.contains(result, "apple\nBanana\nOrange\n")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test/")
        shutil.rmtree("test")
        Step("收尾工作")
