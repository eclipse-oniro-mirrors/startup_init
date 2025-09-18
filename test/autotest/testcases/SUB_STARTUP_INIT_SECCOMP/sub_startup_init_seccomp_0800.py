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

from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import *
from aw import Common


class SubStartupInitSeccomp0800(TestCase):

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
        Step("查询hdcd进程.................")
        pid = self.driver.shell("ps -ef | grep hdcd")
        pid = pid.split()[1]
        Step(pid)
        Step("查询hdcd的seccomp使能状态.................")
        result = self.driver.shell("cat /proc/%s/status | grep Seccomp" % (pid))
        Step(result)
        self.driver.Assert.contains(result, "0", "初始Seccomp标志位不为0")
        Step("导入文件libhdcd_filter.z.so.................")
        self.driver.hdc("target mount")
        sourpath_se = Common.sourcepath("libhdcd_filter.z.so", "SUB_STARTUP_INIT_SECCOMP")
        destpath_se = "/system/lib64/seccomp/libhdcd_filter.z.so"
        self.driver.push_file(sourpath_se, destpath_se)
        sleep(3)
        Step("重启生效..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
        Step("重启后查询hdcd进程.................")
        pid = self.driver.shell("ps -ef | grep hdcd")
        pid = pid.split()[1]
        Step(pid)
        Step("重启后查询hdcd的seccomp使能状态.................")
        result_reb = self.driver.shell("cat /proc/%s/status | grep Seccomp" % (pid))
        Step(result_reb)
        sleep(3)
        self.driver.Assert.contains(result_reb, "2", "重启后Seccomp标志位不为2")

    def teardown(self):
        Step("收尾工作.................")
        self.driver.hdc("target mount")
        self.driver.shell("rm -rf /system/lib64/seccomp/libhdcd_filter.z.so")
        Step("重启恢复..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
