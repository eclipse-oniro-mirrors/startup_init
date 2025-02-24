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
import re
from hypium import UiDriver
from datetime import datetime
from aw import Common
from devicetest.core.test_case import Step, TestCase


class SubStartupToyboxTouchtest1100(TestCase):

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

    def test_step1(self):
        Step("显示帮助命令")
        result = self.driver.shell(f"help tar")
        self.driver.Assert.contains(result, "usage: tar [-cxt] [-fvohmjkOS] [-XTCf NAME] [FILES]")
        self.driver.Assert.contains(result, "Create, extract, or list files in a .tar ")
        self.driver.Assert.contains(result, "c  Create                x  Extract               t  Test (list)")
        self.driver.Assert.contains(result, "f  tar FILE (default -)  C  Change to DIR first   v  Verbose display")
        self.driver.Assert.contains(result, "o  Ignore owner          h  Follow symlinks       m  Ignore mtime")
        self.driver.Assert.contains(result, "J  xz compression        j  bzip2 compression     z  gzip compression")
        self.driver.Assert.contains(result,
                                    "O  Extract to stdout     X  exclude names in FILE T  include names in FILE")

        result = self.driver.shell(f"tar --help")
        self.driver.Assert.contains(result, "usage: tar [-cxt] [-fvohmjkOS] [-XTCf NAME] [FILES]")
        self.driver.Assert.contains(result, "Create, extract, or list files in a .tar ")
        self.driver.Assert.contains(result, "c  Create                x  Extract               t  Test (list)")
        self.driver.Assert.contains(result, "f  tar FILE (default -)  C  Change to DIR first   v  Verbose display")
        self.driver.Assert.contains(result, "o  Ignore owner          h  Follow symlinks       m  Ignore mtime")
        self.driver.Assert.contains(result, "J  xz compression        j  bzip2 compression     z  gzip compression")
        self.driver.Assert.contains(result,
                                    "O  Extract to stdout     X  exclude names in FILE T  include names in FILE")

        self.driver.shell(f"mkdir -p {self.usr_workspace}/basedata/test")
        self.driver.shell(f"mkdir -p {self.usr_workspace}/basedata/test2")
        self.driver.shell(f"mkdir -p {self.usr_workspace}/basedata/result")
        self.driver.shell(f"echo  Hello, World! > {self.usr_workspace}/basedata/test/file.txt")
        self.driver.shell(f"echo  Hello, World! > {self.usr_workspace}/basedata/test/file2.txt")

        Step("创建归档文件")
        result = self.driver.shell(f"tar -cvf {self.usr_workspace}/basedata/result/testarchive.tar {self.usr_workspace}/basedata/test/")
        self.driver.Assert.contains(result, f"basedata/test/")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/basedata/result")
        self.driver.Assert.contains(result, "testarchive.tar")
        self.driver.shell(f"rm -rf {self.usr_workspace}/basedata/result/*")

        Step("打包时更改到指定目录")
        result = self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/test2.tar -C /data/local/tmp/basedata/test/ .")
        self.driver.Assert.contains(result, "file.txt")
        self.driver.Assert.contains(result, "file2.txt")
        self.driver.Assert.is_true("basedata" not in result)
        result = self.driver.shell(f"ls -l {self.usr_workspace}/basedata/result")
        self.driver.Assert.contains(result, "test2.tar")
        self.driver.shell(f"rm -rf {self.usr_workspace}/basedata/result/*")

        Step("创建归档文件，并且将文件权限设置为777")
        result = self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/testfile.tar --mode=777 {self.usr_workspace}/basedata/test/file.txt")
        self.driver.Assert.contains(result, "basedata/test/file.txt")

    def test_step2(self):
        # 先删除压缩前的文件，避免影响后续测试
        self.driver.shell(f"rm {self.usr_workspace}/basedata/test/file.txt")
        self.driver.shell(f"tar -xvf {self.usr_workspace}/basedata/result/testfile.tar")
        result = self.driver.shell(f"stat {self.usr_workspace}/basedata/test/file.txt")
        result = Common.getPermission(result)
        self.driver.Assert.contains(result, "777")
        self.driver.shell(f"rm -rf {self.usr_workspace}/basedata/result/*")

        Step("忽略文件的修改时间")
        result = self.driver.shell(
            f"tar -cvmf {self.usr_workspace}/basedata/result/testignoretime.tar {self.usr_workspace}/basedata/test/")
        self.driver.Assert.contains(result, "basedata/test/file.txt")
        result = self.driver.shell(f"tar -tvf {self.usr_workspace}/basedata/result/testignoretime.tar")
        self.driver.Assert.contains(result, "basedata/test/file.txt")

        Step("列出归档文件内容,并显示时间")
        self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/testarchive.tar {self.usr_workspace}/basedata/test/")
        self.driver.shell(f"rm -rf {self.usr_workspace}/basedata/test/*")
        result = self.driver.shell(f"tar -tvf {self.usr_workspace}/basedata/result/testarchive.tar")
        # 获取当前时间
        current_time = datetime.now()
        # 从当前时间中提取年份
        current_year = current_time.year
        self.driver.Assert.contains(result, str(current_year))
        self.driver.Assert.contains(result, "basedata/test/file.txt")
        self.driver.shell(f"rm -rf {self.usr_workspace}/basedata/result/*")

        Step("提取归档文件")
        self.driver.shell(f"mkdir -p {self.usr_workspace}/basedata/test2/subdir")
        self.driver.shell(f"echo  Hello, World! > {self.usr_workspace}/basedata/test2/file.txt")
        self.driver.shell(f"echo  Hello, World! > {self.usr_workspace}/basedata/test2/file2.txt")
        self.driver.shell(f"echo  Hello, World! > {self.usr_workspace}/basedata/test2/subdir/file.txt")

        result = self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/test2.tar {self.usr_workspace}/basedata/test2/")
        self.driver.Assert.contains(result, f"basedata/test2/")

        self.driver.shell(f"rm -rf {self.usr_workspace}/basedata/test2/*")
        result = self.driver.shell(f"tar -xvf {self.usr_workspace}/basedata/result/test2.tar")
        self.driver.Assert.contains(result, "test2/subdir/")
        self.driver.Assert.contains(result, "test2/file.txt")
        self.driver.shell(f"rm -rf {self.usr_workspace}/basedata/result/*")

    def test_step3(self):
        Step("提取归档文件,将提取的文件输出到标准输出")
        self.driver.shell(f"echo  Hello, World! > {self.usr_workspace}/basedata/test/file.txt")
        self.driver.shell(f"echo  Hello, World! > {self.usr_workspace}/basedata/test/file2.txt")
        self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/testarchive.tar {self.usr_workspace}/basedata/test/file.txt")
        self.driver.shell(f"rm -rf {self.usr_workspace}/basedata/test/*")
        result = self.driver.shell(f"tar -xOvf {self.usr_workspace}/basedata/result/testarchive.tar")
        self.driver.Assert.contains(result, "Hello, World!")
        result = self.driver.shell(f"ls {self.usr_workspace}/basedata/test/")
        self.driver.Assert.is_true(result == "")

        Step("使用gzip压缩选项")
        result = self.driver.shell(f"tar -czvf {self.usr_workspace}/basedata/result/testarchive.tar.gz {self.usr_workspace}/basedata/test2/")
        self.driver.Assert.contains(result, "test2/file.txt")
        self.driver.Assert.contains(result, "test2/file2.txt")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/basedata/result")
        self.driver.Assert.contains(result, "testarchive.tar.gz")

        Step("排除特定文件")
        self.driver.shell(f"echo  Hello, World! > {self.usr_workspace}/basedata/test/file.txt")
        self.driver.shell(f"echo  Hello, World! > {self.usr_workspace}/basedata/test/file2.txt")
        result = self.driver.shell(
            f"tar -xvf {self.usr_workspace}/basedata/testarchive.tar "
            f"--exclude='{self.usr_workspace}/basedata/test/file2.txt'")
        self.driver.Assert.is_true("file2.txt" not in result)

        self.driver.shell(f"echo {self.usr_workspace}/basedata/test2/subdir/ > {self.usr_workspace}/basedata/include.txt")
        result = self.driver.shell(
            f"tar -xvf {self.usr_workspace}/basedata/result/testarchive.tar -X {self.usr_workspace}/basedata/include.txt")
        self.driver.Assert.contains(result, "file.txt")
        self.driver.Assert.is_true("subdir" not in result)
        self.driver.shell(f"rm {self.usr_workspace}/basedata/include.txt")

        Step("包含特定文件")
        self.driver.shell(f"echo storage/media/100/local/files/Docs/basedata/test2/subdir/file.txt > {self.usr_workspace}/basedata/include.txt")
        self.driver.shell(f"echo storage/media/100/local/files/Docs/basedata/test2/file.txt >> {self.usr_workspace}/basedata/include.txt")
        result = self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/testarchive_include.tar -T {self.usr_workspace}/basedata/include.txt")
        self.driver.Assert.contains(result, "file.txt")
        self.driver.Assert.contains(result, "subdir")
        self.driver.Assert.is_true("file2" not in result)
        self.driver.shell(f"rm {self.usr_workspace}/basedata/include.txt")

    def test_step4(self):
        Step("显示完整时间信息")
        self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/testarchive.tar {self.usr_workspace}/basedata/test/")
        result = self.driver.shell(
            f"tar -tvf {self.usr_workspace}/basedata/result/testarchive.tar --full-time")
        result = Common.getPattern(result)
        self.driver.Assert.is_true(result == True)

        Step("覆盖文件的时间戳")
        result = self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/specified_time.tar --mtime='2024-01-01' {self.usr_workspace}/basedata/test/file.txt")
        self.driver.Assert.contains(result, "basedata/test/file.txt")
        self.driver.shell(f"tar -xvf {self.usr_workspace}/basedata/result/specified_time.tar")
        result = self.driver.shell(f"stat {self.usr_workspace}/basedata/test/file.txt")
        self.driver.Assert.contains(result, "Modify: 2024-01-01")

        Step("设置文件的所有者为指定的名称并记录稀疏文件")
        result = self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/specified.tar --owner=2000 --sparse {self.usr_workspace}/basedata/test/file.txt")
        self.driver.Assert.contains(result, "basedata/test/file.txt")
        self.driver.shell(f"tar -xvf {self.usr_workspace}/basedata/result/specified.tar")
        result = self.driver.shell(f"stat {self.usr_workspace}/basedata/test/file.txt")
        self.driver.Assert.contains(result, "2000")

        Step("不存储目录内容")
        result = self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/archive.tar --no-recursion {self.usr_workspace}/basedata/test/")
        self.driver.Assert.contains(result, "basedata/test/")
        self.driver.Assert.is_true("file.txt" not in result)

        Step("所有归档内容必须提取到一个子目录中")
        self.driver.shell(
            f"tar -cvf {self.usr_workspace}/basedata/result/archive.tar {self.usr_workspace}/basedata/test/")
        self.driver.shell(f"mkdir {self.usr_workspace}/basedata/another_dir")
        self.driver.shell(f"cd {self.usr_workspace}/basedata/another_dir && "
                          f"tar --restrict -xvf {self.usr_workspace}/basedata/result/archive.tar")
        result = self.driver.shell(f"ls {self.usr_workspace}/basedata/another_dir")
        self.driver.Assert.contains(result, f"data")

    def teardown(self):
        Step("收尾工作")
        self.driver.shell(f"rm -rf {self.usr_workspace}/basedata/")
