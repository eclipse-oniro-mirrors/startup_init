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
from hypium import *


class SUB_STARTUP_INIT_SERMGT_1000(TestCase):

    def __init__(self, controllers):
        self.TAG = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step("预置工作:初始化手机开始.................")
        Step(self.devices[0].device_id)

    def test_step1(self):
        Step("重命名系统文件watchdog.cfg..........................")
        self.driver.hdc("target mount")
        self.driver.shell(
            "cp /system/etc/init/watchdog.cfg /system/etc/init/watchdog.cfg_bak")
        Step("把系统文件watchdog.cfg拉到本地..........................")
        devpath = "/system/etc/init/watchdog.cfg"
        self.driver.pull_file(devpath, "testFile/SUB_STARTUP_INIT_SERMGT/watchdog.cfg")
        Step("修改本地系统文件..........................")
        lines1 = []
        path2 = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
                             "testFile/SUB_STARTUP_INIT_SERMGT")
        localPath = os.path.join(path2, "watchdog.cfg")
        with open(localPath, "r") as y:
            for line in y:
                lines1.append(line)
            y.close()
        Step(lines1)
        line = lines1.index('            "uid" : "watchdog",\n')
        list = '\t\t\t"start-mode" : "boot",\n\t\t\t"uid" : "root",\n\t\t\t"caps" : ["DAC_OVERRIDE"],\n'
        lines1.insert(line, list)
        lines1.remove('            "start-mode" : "condition",\n')
        lines1.remove('            "uid" : "watchdog",\n')
        s = ''.join(lines1)
        Step(s)
        with open(localPath, "w") as z:
            z.write(s)
            z.close()
        Step("上传修改后的本地系统文件..........................")
        self.driver.push_file(localPath, devpath)
        Step("执行重启..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
        Step("获取watchdog、appspawn的pid..........................")
        pidofwatchdog = self.driver.System.get_pid("watchdog_service")
        Step(pidofwatchdog)
        pidofappspawn = self.driver.System.get_pid("appspawn")
        Step(pidofappspawn)
        Step("获取init、watchdog、appspawn的Capability权限..........................")
        capsofinit = self.driver.shell('cat proc/1/status |grep Cap')
        Step(capsofinit)
        capsofwatchdog = self.driver.shell('cat proc/' + str(pidofwatchdog) + '/status |grep Cap')
        Step(capsofwatchdog)
        capsofappspawn = self.driver.shell('cat proc/' + str(pidofappspawn) + '/status |grep Cap')
        Step(capsofappspawn)
        Step("init、appspawn进程具备的相同的Capability权限，watchdog具备不同的Capability权限..........................")
        self.driver.Assert.equal(capsofappspawn, capsofinit)
        self.driver.Assert.not_equal(capsofwatchdog, capsofinit)

    def teardown(self):
        Step("收尾工作.................")
        Step("恢复系统文件watchdog.cfg..........................")
        self.driver.hdc("target mount")
        self.driver.shell("rm -rf /system/etc/init/watchdog.cfg")
        self.driver.shell(
            "mv /system/etc/init/watchdog.cfg_bak /system/etc/init/watchdog.cfg")
        Step("执行重启..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
