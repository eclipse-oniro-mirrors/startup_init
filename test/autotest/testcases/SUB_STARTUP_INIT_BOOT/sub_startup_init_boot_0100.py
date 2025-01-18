#
# Copyright (c) 2024 Huawei Device Co., Ltd.
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

import os
# -*- coding: utf-8 -*-
from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import UiDriver


class SubStartupInitBoot0100(TestCase):

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
        Step("重命名系统文件useriam.cfg..........................")
        self.driver.hdc("target mount")
        self.driver.shell(
            "cp /etc/init/useriam.cfg /etc/init/useriam.cfg_bak")
        Step("把系统文件useriam.cfg拉到本地..........................")
        devpath = "/etc/init/useriam.cfg"
        self.driver.pull_file(devpath, "testFile/SUB_STARTUP_INIT_BOOT/useriam.cfg")
        Step("修改本地系统文件..........................")
        lines1 = []
        path2 = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
                             "testFile/SUB_STARTUP_INIT_BOOT")
        localpath = os.path.join(path2, "useriam.cfg")
        with open(localpath, "r") as y:
            for line in y:
                lines1.append(line)
            y.close()
        Step(lines1)
        line = lines1.index('    "services" : [{\n') + 1
        d_caps_entry = '\t\t\t"bootevents" : ["bootevent.useriam.fwkready"],\n'
        lines1.insert(line, d_caps_entry)
        s = ''.join(lines1)
        Step(s)
        line = lines1.index('{\n') + 1
        d_caps_entry = [
            '\t],\n',
            '\t\t}\n',
            '\t\t\t"cmds" : ["mkdir /data/service/el1/public/userauth/ 0700 useriam useriam"]\n',
            '\t\t\t"name" : "service:useriam",\n',
            '\t"jobs" : [{\n'
        ]
        for i in d_caps_entry:
            lines1.insert(line, i)
        s = ''.join(lines1)
        Step(s)
        with open(localpath, "w") as z:
            z.write(s)
            z.close()
        Step("上传修改后的本地系统文件..........................")
        self.driver.push_file(localpath, devpath)
        Step("执行重启..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
        Step("执行命令param get bootevent.boot.completed..........................")
        result = self.driver.shell('param get bootevent.boot.completed')
        Step(result)
        self.driver.Assert.contains(result, 'true')

    def teardown(self):
        Step("收尾工作.................")
        Step("恢复系统文件useriam.cfg..........................")
        self.driver.hdc("target mount")
        self.driver.shell("rm -rf /etc/init/useriam.cfg")
        self.driver.shell(
            "mv /etc/init/useriam.cfg_bak /etc/init/useriam.cfg")
        Step("执行重启..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
