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
import time
from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import UiDriver
from aw import Common
from hypium import UiDriver, UiExplorer
from hypium.action.os_hypium.device_logger import DeviceLogger
import subprocess


class SubStartupInitAccessIsolation0100(TestCase):

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
        Step("导入测试文件systemapp.................")
        self.driver.hdc("target mount")
        sourpath_se = Common.sourcepath("systemapp", "sub_startup_init_access_isolation")
        destpath_se = "system/bin/systemapp"
        self.driver.push_file(sourpath_se, destpath_se)
        sleep(3)
        Step("重启生效..........................")
        self.driver.System.reboot()
        sleep(5)
        self.driver.System.wait_for_boot_complete()

    def test_step1(self):
        Step("过滤打印hilog日志.................")
        device_logger = DeviceLogger(self.driver)
        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        device_logger.set_filter_string("systemapp")
        Step("开始抓取日志")
        device_logger.start_log(path + '\\testFile\\log\\%s.log' % (self.TAG))
        Step("给测试文件systemapp添加可执行权限.................")
        self.driver.hdc("target mount")
        self.driver.shell("chmod +x ./system/bin/systemapp")
        Step("防止hilog日志出现unknown log.................")
        self.driver.shell("hilog -d /system/bin/systemapp")
        Step("执行测试文件systemapp.................")
        self.driver.shell("./system/bin/systemapp")
        time.sleep(3)
        Step("停止抓日志")
        device_logger.stop_log()
        Step("断言预期结果")
        time.sleep(2)
        device_logger.check_log('systemapp:prohibit system component processes from accessing vendor files')

    def teardown(self):
        Step("收尾工作.................")
        self.driver.hdc("target mount")
        self.driver.shell("rm -rf system/bin/systemapp")
        Step("重启恢复..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()
