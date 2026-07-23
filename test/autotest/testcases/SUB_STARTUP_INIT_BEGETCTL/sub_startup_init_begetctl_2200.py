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

# -*- coding: utf-8 -*-
from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import UiDriver


class SunStartupInitBegetctl2200(TestCase):

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
        Step("查看root组的gid.................")
        result_root_gid = self.driver.shell('begetctl dac gid root')
        Step(result_root_gid)
        Step("检查root的gid结果.................")
        self.driver.Assert.contains(result_root_gid, 'getgrnam gid root')
        self.driver.Assert.contains(result_root_gid, 'getgrent gid root')
        sleep(1)
        Step("查看system组的gid.................")
        result_system_gid = self.driver.shell('begetctl dac gid system')
        Step(result_system_gid)
        Step("检查system的gid结果.................")
        self.driver.Assert.contains(result_system_gid, 'getgrnam gid system')
        self.driver.Assert.contains(result_system_gid, 'getgrent gid system')
        sleep(1)
        Step("查看shell组的gid.................")
        result_shell_gid = self.driver.shell('begetctl dac gid shell')
        Step(result_shell_gid)
        Step("检查shell的gid结果.................")
        self.driver.Assert.contains(result_shell_gid, 'getgrnam gid shell')
        self.driver.Assert.contains(result_shell_gid, 'getgrent gid shell')
        sleep(1)
        Step("查看radio组的gid.................")
        result_radio_gid = self.driver.shell('begetctl dac gid radio')
        Step(result_radio_gid)
        Step("检查radio的gid结果.................")
        self.driver.Assert.contains(result_radio_gid, 'getgrnam gid radio')
        self.driver.Assert.contains(result_radio_gid, 'getgrent gid radio')

    def teardown(self):
        Step("收尾工作.................")