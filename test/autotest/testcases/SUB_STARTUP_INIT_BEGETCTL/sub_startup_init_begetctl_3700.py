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


class SunStartupInitBegetctl3700(TestCase):

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
        Step("设置测试参数test.case.001.................")
        result_001 = self.driver.shell('begetctl param set test.case.001 test_value_001')
        Step(result_001)
        self.driver.Assert.contains(result_001, 'Set parameter test.case.001 test_value_001 success')
        sleep(1)
        Step("设置测试参数test.case.002.................")
        result_002 = self.driver.shell('begetctl param set test.case.002 test_value_002')
        Step(result_002)
        self.driver.Assert.contains(result_002, 'Set parameter test.case.002 test_value_002 success')
        sleep(1)
        Step("设置测试参数test.case.003.................")
        result_003 = self.driver.shell('begetctl param set test.case.003 test_value_003')
        Step(result_003)
        self.driver.Assert.contains(result_003, 'Set parameter test.case.003 test_value_003 success')
        sleep(1)
        Step("设置测试参数test.case.004.................")
        result_004 = self.driver.shell('begetctl param set test.case.004 test_value_004')
        Step(result_004)
        self.driver.Assert.contains(result_004, 'Set parameter test.case.004 test_value_004 success')
        sleep(1)
        Step("设置测试参数test.case.005.................")
        result_005 = self.driver.shell('begetctl param set test.case.005 test_value_005')
        Step(result_005)
        self.driver.Assert.contains(result_005, 'Set parameter test.case.005 test_value_005 success')

    def teardown(self):
        Step("收尾工作.................")