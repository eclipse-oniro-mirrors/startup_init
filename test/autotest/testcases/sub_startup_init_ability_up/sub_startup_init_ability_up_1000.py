#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2025 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import UiDriver


class SubStartupInitAbilityUp1000(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.tag, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step("预置工作:初始化手机开始.................")
        Step(self.devices[0].device_id)

    def test_step1(self):
        Step("重命名系统文件hilogd.cfg..........................")
        self.driver.hdc("target mount")
        self.driver.shell(
            "cp /system/etc/init/hilogd.cfg /system/etc/init/hilogd.cfg_bak")
        Step("把系统文件hilogd.cfg拉到本地..........................")
        devpath = "/system/etc/init/hilogd.cfg"
        self.driver.pull_file(devpath, "testFile/sub_startup_init_ability_up/hilogd.cfg")
        Step("修改本地系统文件..........................")
        lines1 = []
        path2 = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
                             "testFile\sub_startup_init_ability_up")
        local_path = os.path.join(path2, "hilogd.cfg")
        with open(local_path, "r") as y:
            for line in y:
                lines1.append(line)
            y.close()
        line = lines1.index('            "secon" : "u:r:hilogd:s0"\n')
        res1 = line
        Step(f"目标索引：{res1}")
        index_str1 = lines1[res1]
        Step(f"需要删除目标：{index_str1}")
        lines1.remove(index_str1)
        line2 = line - 1
        index_str2 = lines1[line2]
        lines2 = list(index_str2)
        Step(f"需要修改,：{lines2}")
        if ',' in lines2:
            lines2.remove(',')
            s2 = ''.join(lines2)
            Step(f"修改,之后：{s2}")
        else:
            pass
        lines1.remove(index_str2)
        lines1.insert(line2, s2)
        Step(f"全部修改：{lines1}")
        s = ''.join(lines1)
        Step(f"修改之后的文件内容：{s}")
        with open(local_path, "w") as z:
            z.write(s)
            z.close()
        Step("上传修改后的本地系统文件..........................")
        self.driver.push_file(local_path, devpath)
        Step("执行重启..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
        Step("执行命令begetctl dump_service..........................")
        result = self.driver.shell('begetctl dump_service hilogd')
        Step(result)
        self.driver.Assert.contains(result, "HiLogAdapter_init: Can't connect to server.")

    def teardown(self):
        Step("收尾工作.................")
        Step("恢复系统文件hilogd.cfg..........................")
        self.driver.hdc("target mount")
        self.driver.shell("rm -rf /system/etc/init/hilogd.cfg")
        self.driver.shell(
            "mv /system/etc/init/hilogd.cfg_bak /system/etc/init/hilogd.cfg")
        Step("执行重启..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
