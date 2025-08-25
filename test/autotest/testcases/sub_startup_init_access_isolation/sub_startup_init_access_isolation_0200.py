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
from hypium import UiDriver
from aw import Common
from hypium import UiDriver, UiExplorer
from hypium.action.os_hypium.device_logger import DeviceLogger
import subprocess


class SubStartupInitAccessIsolation0200(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.tag, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        print("预置工作:初始化手机开始.................")
        print(self.devices[0].device_id)
        Step("导入测试文件chipsetapp.................")
        self.driver.hdc("target mount")
        sourpath_se = Common.sourcepath("chipsetapp", "sub_startup_init_access_isolation")
        destpath_se = "vendor/bin/chipsetapp"
        self.driver.push_file(sourpath_se, destpath_se)
        sleep(3)
        Step("重启生效..........................")
        self.driver.System.reboot()
        sleep(5)
        self.driver.System.wait_for_boot_complete()

    def test_step1(self):
        Step("过滤打印hilog日志.................")
        cmd = 'hdc shell hilog | grep chipsetapp'
        trace = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=10)
        count = 0
        Step("给测试文件chipsetapp添加可执行权限.................")
        self.driver.hdc("target mount")
        self.driver.shell("chmod +x ./vendor/bin/chipsetapp")
        Step("执行测试文件chipsetapp.................")
        self.driver.shell("./vendor/bin/chipsetapp")
        Step("验证执行结果.................")
        while trace.poll() is None:
            result_text = trace.stdout.readline().decode()
            if 'chipsetapp:prohibit chipset component processes from accessing system files' in result_text:
                Step("实现芯片组件访问系统文件隔离.................")
                trace.terminate()
                break
            count += 1

    def teardown(self):
        print("收尾工作.................")
        self.driver.hdc("target mount")
        self.driver.shell("rm -rf vendor/bin/chipsetapp")
        Step("重启恢复..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
