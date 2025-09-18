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


class SubStartupInitSeccomp0400(TestCase):

    def __init__(self, controllers):
        self.TAG = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.driver = UiDriver(self.device1)

    def test_step1(self):
        Step("查询foundation进程.................")
        pid = self.driver.shell("ps -ef | grep fou")
        pid = pid.split()[1]
        Step(pid)
        Step("查询foundation的seccomp使能状态.................")
        result = self.driver.shell("cat /proc/%s/status | grep Seccomp" % (pid))
        self.driver.Assert.contains(result, "2", "初始Seccomp标志位不为2")
        Step("修改系统参数..........................")
        self.driver.shell("param set persist.init.debug.seccomp.enable 0")
        Step("重启生效..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
        Step("查询foundation进程.................")
        pid = self.driver.shell("ps -ef | grep fou")
        pid = pid.split()[1]
        Step(pid)
        Step("重启后查询foundation的seccomp使能状态.................")
        result_reb = self.driver.shell("cat /proc/%s/status | grep Seccomp" % (pid))
        self.driver.Assert.contains(result_reb, "0", "重启后Seccomp标志位不为0")

    def teardown(self):
        Step("收尾工作.................")
        self.driver.shell("param set persist.init.debug.seccomp.enable 1")
        Step("重启恢复..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
