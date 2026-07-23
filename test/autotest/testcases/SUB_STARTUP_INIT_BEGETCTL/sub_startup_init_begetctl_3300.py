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


class SunStartupInitBegetctl3300(TestCase):

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
        Step("查看root用户uid.................")
        result_root = self.driver.shell('begetctl dac uid root')
        Step(result_root)
        self.driver.Assert.contains(result_root, 'getpwnam uid root')
        self.driver.Assert.contains(result_root, 'getpwent uid root')
        sleep(1)
        Step("查看system用户uid.................")
        result_system = self.driver.shell('begetctl dac uid system')
        Step(result_system)
        self.driver.Assert.contains(result_system, 'getpwnam uid system')
        self.driver.Assert.contains(result_system, 'getpwent uid system')
        sleep(1)
        Step("查看shell用户uid.................")
        result_shell = self.driver.shell('begetctl dac uid shell')
        Step(result_shell)
        self.driver.Assert.contains(result_shell, 'getpwnam uid shell')
        self.driver.Assert.contains(result_shell, 'getpwent uid shell')
        sleep(1)
        Step("查看radio用户uid.................")
        result_radio = self.driver.shell('begetctl dac uid radio')
        Step(result_radio)
        self.driver.Assert.contains(result_radio, 'getpwnam uid radio')
        self.driver.Assert.contains(result_radio, 'getpwent uid radio')
        sleep(1)
        Step("查看wifi用户uid.................")
        result_wifi = self.driver.shell('begetctl dac uid wifi')
        Step(result_wifi)
        self.driver.Assert.contains(result_wifi, 'getpwnam uid wifi')
        self.driver.Assert.contains(result_wifi, 'getpwent uid wifi')

    def teardown(self):
        Step("收尾工作.................")