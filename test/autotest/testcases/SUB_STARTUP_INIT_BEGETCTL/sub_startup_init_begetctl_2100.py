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


class SunStartupInitBegetctl2100(TestCase):

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
        Step("查看root用户的uid.................")
        result_root_uid = self.driver.shell('begetctl dac uid root')
        Step(result_root_uid)
        Step("检查root的uid结果.................")
        self.driver.Assert.contains(result_root_uid, 'getpwnam uid root')
        self.driver.Assert.contains(result_root_uid, 'getpwent uid root')
        sleep(1)
        Step("查看system用户的uid.................")
        result_system_uid = self.driver.shell('begetctl dac uid system')
        Step(result_system_uid)
        Step("检查system的uid结果.................")
        self.driver.Assert.contains(result_system_uid, 'getpwnam uid system')
        self.driver.Assert.contains(result_system_uid, 'getpwent uid system')
        sleep(1)
        Step("查看shell用户的uid.................")
        result_shell_uid = self.driver.shell('begetctl dac uid shell')
        Step(result_shell_uid)
        Step("检查shell的uid结果.................")
        self.driver.Assert.contains(result_shell_uid, 'getpwnam uid shell')
        self.driver.Assert.contains(result_shell_uid, 'getpwent uid shell')
        sleep(1)
        Step("查看radio用户的uid.................")
        result_radio_uid = self.driver.shell('begetctl dac uid radio')
        Step(result_radio_uid)
        Step("检查radio的uid结果.................")
        self.driver.Assert.contains(result_radio_uid, 'getpwnam uid radio')
        self.driver.Assert.contains(result_radio_uid, 'getpwent uid radio')

    def teardown(self):
        Step("收尾工作.................")