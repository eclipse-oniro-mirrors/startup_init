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
import time
from hypium import UiDriver
from devicetest.core.test_case import Step, TestCase
from aw import Common


class SubStartupToyboxTouchtest0100(TestCase):

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
        if os.path.exists("test") != True:
            os.mkdir("test")

    def test_step1(self):
        Step("显示帮助命令")
        result = self.driver.shell(f"help cp")
        self.driver.Assert.contains(result, "cp [-adfHiLlnPpRrsTv] [--preserve=motcxa] [-t TARGET] SOURCE... [DEST]")
        self.driver.Assert.contains(result, "Copy files from SOURCE to DEST.  If more than one SOURCE, DEST must")
        self.driver.Assert.contains(result, "be a directory.")
        self.driver.Assert.contains(result, "-v	Verbose")
        self.driver.Assert.contains(result, "-s	Symlink instead of copy")
        self.driver.Assert.contains(result, "-r	Synonym for -R")
        self.driver.Assert.contains(result, "-n	No clobber (don't overwrite DEST)")
        self.driver.Assert.contains(result, "-l	Hard link instead of copy")
        self.driver.Assert.contains(result, "-d	Don't dereference symlinks")
        self.driver.Assert.contains(result, "-a	Same as -dpr")
        self.driver.Assert.contains(result, "-P	Do not follow symlinks")
        self.driver.Assert.contains(result, "-L	Follow all symlinks")
        self.driver.Assert.contains(result, "-H	Follow symlinks listed on command line")
        self.driver.Assert.contains(result, "-R	Recurse into subdirectories (DEST must be a directory)")
        self.driver.Assert.contains(result, "-p	Preserve timestamps, ownership, and mode")
        self.driver.Assert.contains(result, "-i	Interactive, prompt before overwriting existing DEST")
        self.driver.Assert.contains(result, "-F	Delete any existing destination file first (--remove-destination)")
        self.driver.Assert.contains(result, "-f	Delete destination files we can't write to")
        self.driver.Assert.contains(result, "-D	Create leading dirs under DEST (--parents)")
        self.driver.Assert.contains(result, "-t	Copy to TARGET dir (no DEST)")
        self.driver.Assert.contains(result, "-T	DEST always treated as file, max 2 arguments")

        result = self.driver.shell(f"cp --help")
        self.driver.Assert.contains(result, "cp [-adfHiLlnPpRrsTv] [--preserve=motcxa] [-t TARGET] SOURCE... [DEST]")
        self.driver.Assert.contains(result, "Copy files from SOURCE to DEST.  If more than one SOURCE, DEST must")
        self.driver.Assert.contains(result, "be a directory.")
        self.driver.Assert.contains(result, "-v	Verbose")
        self.driver.Assert.contains(result, "-s	Symlink instead of copy")
        self.driver.Assert.contains(result, "-r	Synonym for -R")
        self.driver.Assert.contains(result, "-n	No clobber (don't overwrite DEST)")
        self.driver.Assert.contains(result, "-l	Hard link instead of copy")
        self.driver.Assert.contains(result, "-d	Don't dereference symlinks")
        self.driver.Assert.contains(result, "-a	Same as -dpr")
        self.driver.Assert.contains(result, "-P	Do not follow symlinks")
        self.driver.Assert.contains(result, "-L	Follow all symlinks")
        self.driver.Assert.contains(result, "-H	Follow symlinks listed on command line")
        self.driver.Assert.contains(result, "-R	Recurse into subdirectories (DEST must be a directory)")
        self.driver.Assert.contains(result, "-p	Preserve timestamps, ownership, and mode")
        self.driver.Assert.contains(result, "-i	Interactive, prompt before overwriting existing DEST")
        self.driver.Assert.contains(result, "-F	Delete any existing destination file first (--remove-destination)")
        self.driver.Assert.contains(result, "-f	Delete destination files we can't write to")
        self.driver.Assert.contains(result, "-D	Create leading dirs under DEST (--parents)")
        self.driver.Assert.contains(result, "-t	Copy to TARGET dir (no DEST)")
        self.driver.Assert.contains(result, "-T	DEST always treated as file, max 2 arguments")

    def test_step2(self):
        # 创建测试文件
        Common.writeDateToFile("test/original.txt", "Hello, World!")
        Common.writeDateToFile("test/original1.txt", "Hello, World!")
        Common.writeDateToFile("test/original3.txt", "Hello, World!")
        Common.writeDateToFile("test/original4.txt", "Hello, World!")
        Common.writeDateToFile("test/original5.txt", "Original content")
        Common.writeDateToFile("test/original2.txt", "She danced under the moonlit sky, her laughter echoing.")
        self.driver.push_file(f"{os.getcwd()}/test", self.usr_workspace)
        Step("复制单个文件")
        self.driver.shell(f"cp {self.usr_workspace}/test/original.txt {self.usr_workspace}/test/destination.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/destination.txt")
        self.driver.Assert.contains(result, "Hello, World!")
        Step("不覆盖目标文件")
        self.driver.shell(
            f"touch {self.usr_workspace}/test/test.txt")
        self.driver.shell(f"cp -n {self.usr_workspace}/test/test.txt {self.usr_workspace}/test/destination.txt")
        self.driver.shell(f"cat {self.usr_workspace}/test/destination.txt")
        self.driver.Assert.contains(result, "Hello, World!")
        self.driver.shell(f"rm {self.usr_workspace}/test/destination.txt")
        Step("复制文件夹并显示详细信息")
        self.driver.shell(f"mkdir {self.usr_workspace}/test2")
        result = self.driver.shell(
            f"cp -vr {self.usr_workspace}/test/ {self.usr_workspace}/test2/")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test2/test")
        self.driver.Assert.contains(result, "original3.txt")
        self.driver.Assert.contains(result, "original2.txt")
        self.driver.Assert.contains(result, "original1.txt")
        self.driver.Assert.contains(result, "original.txt")
        self.driver.shell(f"rm -rf {self.usr_workspace}/test2/")
        Step("创建符号链接")
        self.driver.shell(
            f"cp -s {self.usr_workspace}/test/original.txt {self.usr_workspace}/test/testlink")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.contains(result, f"testlink -> {self.usr_workspace}/test/original.txt")
        Step("创建硬链接")
        self.driver.shell(
            f"cp -l {self.usr_workspace}/test/original1.txt {self.usr_workspace}/test/testhardlink")
        self.driver.shell(
            f"cp -l {self.usr_workspace}/test/testhardlink {self.usr_workspace}/test/testhardlink1")
        result = self.driver.shell(f"stat {self.usr_workspace}/test/testhardlink1")
        self.driver.Assert.contains(result, f"Links: 3")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/testhardlink")
        self.driver.Assert.contains(result, "Hello, World!")

    def test_step3(self):
        # 修改目标文件内容
        self.driver.shell(f"echo This is a test file {self.usr_workspace}/test/original1.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/testhardlink")
        self.driver.Assert.contains(result, "Hello, World!")
        self.driver.shell(f"rm {self.usr_workspace}/test/testlink")
        self.driver.shell(f"rm {self.usr_workspace}/test/testhardlink")
        Step("不解引用符号链接")
        self.driver.shell(f"ln -s {self.usr_workspace}/test/original3.txt {self.usr_workspace}/test/symlink.txt")
        self.driver.shell(
            f"cp -d {self.usr_workspace}/test/symlink.txt {self.usr_workspace}/test/copy_symlink.txt")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.contains(result, f"copy_symlink.txt -> {self.usr_workspace}/test/original3.txt")
        Step("保留文件属性并递归复制")
        self.driver.shell(f"mkdir {self.usr_workspace}/test2")
        self.driver.shell(
            f"cp -a {self.usr_workspace}/test/ {self.usr_workspace}/test2/")
        result1 = self.driver.shell(f"ls -ll {self.usr_workspace}/test2/test/original3.txt")
        result2 = self.driver.shell(f"ls -ll {self.usr_workspace}/test/original3.txt")
        res1 = result1.split()[6]
        res2 = result2.split()[6]
        self.driver.Assert.equal(res1, res2)
        self.driver.shell(f"rm -rf {self.usr_workspace}/test2/")
        Step("复制文件不跟随符号链接")
        self.driver.shell(f"ln -s {self.usr_workspace}/test/original4.txt {self.usr_workspace}/test/symlink4.txt")
        self.driver.shell(
            f"cp -P {self.usr_workspace}/test/symlink4.txt {self.usr_workspace}/test/copy_symlink_P.txt")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.contains(result, f"copy_symlink_P.txt -> {self.usr_workspace}/test/original4.txt")

        result = self.driver.shell(f"cat {self.usr_workspace}/test/copy_symlink_P.txt")
        self.driver.Assert.contains(result, f"Hello, World!")

        Step("复制文件跟随符号链接")
        self.driver.shell(
            f"cp -L {self.usr_workspace}/test/symlink4.txt {self.usr_workspace}/test/copy_symlink_L.txt")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.is_true("copy_symlink_L.txt ->" not in result)
        result = self.driver.shell(f"cat {self.usr_workspace}/test/copy_symlink_L.txt")
        self.driver.Assert.contains(result, "Hello, World!")

    def test_step4(self):
        Step("保留文件的时间戳、所有权和模式等属性")
        self.driver.shell(f"chmod 777 {self.usr_workspace}/test/original2.txt")
        time.sleep(1)
        self.driver.shell(
            f"cp -p {self.usr_workspace}/test/original2.txt {self.usr_workspace}/test/copy_original2.txt")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.contains(result, "copy_original2")
        result1 = self.driver.shell(f"stat {self.usr_workspace}/test/original2.txt")
        self.driver.Assert.contains(result1, "777")
        result2 = self.driver.shell(f"stat {self.usr_workspace}/test/copy_original2.txt")
        self.driver.Assert.contains(result2, "777")

        Step("替换原有文件")
        self.driver.shell(
            f"cp -F {self.usr_workspace}/test/original.txt {self.usr_workspace}/test/original5.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/original5.txt")
        self.driver.Assert.contains(result, "Hello, World!")

        Step("替换原有文件")
        # 将文件变为不可写
        self.driver.shell(f"chmod 000 {self.usr_workspace}/test/original5.txt")
        self.driver.shell(
            f"cp -f {self.usr_workspace}/test/original2.txt {self.usr_workspace}/test/original5.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/original5.txt")
        self.driver.Assert.contains(result, "She danced under the moonlit sky, her laughter echoing")

        Step("带路径拷贝")
        self.driver.shell(f"mkdir {self.usr_workspace}/dir")
        self.driver.shell(
            f"cp -D {self.usr_workspace}/test/original2.txt {self.usr_workspace}/dir")
        result = self.driver.shell(f"ls {self.usr_workspace}/dir{self.usr_workspace}/test")
        self.driver.Assert.contains(result, "original2.txt")

        Step("将文件变为不可写")
        self.driver.shell(f"chmod 000 {self.usr_workspace}/test/original5.txt")
        self.driver.shell(
            f"cp -f {self.usr_workspace}/test/original.txt {self.usr_workspace}/test/test4/original5.txt")
        result = self.driver.shell(f"ls {self.usr_workspace}/test/test4")
        self.driver.Assert.contains(result, "")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/test4/original5.txt")
        self.driver.Assert.contains(result, "")

        Step("复制跟随命令行中列出的符号链接,而不是在复制时跟随符号链接")
        self.driver.shell(f"ln -s {self.usr_workspace}/test/original.txt {self.usr_workspace}/test/original1_link.txt")
        self.driver.shell(f"ln -s {self.usr_workspace}/test/original2.txt {self.usr_workspace}/test/original2_link.txt")
        self.driver.shell(f"mkdir {self.usr_workspace}/test2")
        self.driver.shell(
            f"cp -H {self.usr_workspace}/test/original1_link.txt "
            f"{self.usr_workspace}/test/original2_link.txt {self.usr_workspace}/test2/")
        result = self.driver.shell(f"ls {self.usr_workspace}/test2")
        self.driver.Assert.contains(result, "original1_link.txt")
        self.driver.Assert.contains(result, "original2_link.txt")

    def test_step5(self):
        Step("复制文件到指定目录")
        self.driver.shell(f"touch {self.usr_workspace}/zsj.txt")
        self.driver.shell(f"cp -t {self.usr_workspace}/test/ {self.usr_workspace}/zsj.txt")
        result = self.driver.shell(f"ls -l {self.usr_workspace}/test/")
        self.driver.Assert.contains(result, "zsj.txt")
        Step("复制文件内容")
        self.driver.shell(f"echo \"this is a good girl\" > {self.usr_workspace}/zsj.txt")
        self.driver.shell(f"cp -T {self.usr_workspace}/zsj.txt {self.usr_workspace}/test/original.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/original.txt")
        self.driver.Assert.contains(result, "this is a good girl")
        Step("仅复制源文件中更新时间较新的文件")
        self.driver.shell(f"cp -u {self.usr_workspace}/zsj.txt {self.usr_workspace}/test/original1.txt")
        self.driver.shell(f"cp -u {self.usr_workspace}/test/original2.txt {self.usr_workspace}/test/original1.txt")
        result = self.driver.shell(f"cat {self.usr_workspace}/test/original1.txt")
        self.driver.Assert.contains(result, "this is a good girl")

    def teardown(self):
        self.driver.shell(f"rm -rf {self.usr_workspace}/test/")
        self.driver.shell(f"rm -rf {self.usr_workspace}/test2/")
        self.driver.shell(f"rm -rf {self.usr_workspace}/dir/")
        self.driver.shell(f"rm -rf {self.usr_workspace}/*.txt")
        shutil.rmtree("test")
        Step("收尾工作")

