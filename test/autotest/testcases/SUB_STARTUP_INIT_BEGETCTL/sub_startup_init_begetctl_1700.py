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


class SunStartupInitBegetctl1700(TestCase):

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
        Step("设置测试参数test.param.001.................")
        result_set_001 = self.driver.shell('begetctl param set test.param.001 value001')
        Step(result_set_001)
        Step("检查设置参数结果.................")
        self.driver.Assert.contains(result_set_001, 'Set parameter test.param.001 value001 success')
        sleep(1)
        Step("获取测试参数test.param.001.................")
        result_get_001 = self.driver.shell('begetctl param get test.param.001')
        Step(result_get_001)
        Step("检查获取参数结果.................")
        self.driver.Assert.contains(result_get_001, 'value001')
        sleep(1)
        Step("设置测试参数test.param.002.................")
        result_set_002 = self.driver.shell('begetctl param set test.param.002 value002')
        Step(result_set_002)
        Step("检查设置参数结果.................")
        self.driver.Assert.contains(result_set_002, 'Set parameter test.param.002 value002 success')
        sleep(1)
        Step("获取测试参数test.param.002.................")
        result_get_002 = self.driver.shell('begetctl param get test.param.002')
        Step(result_get_002)
        Step("检查获取参数结果.................")
        self.driver.Assert.contains(result_get_002, 'value002')
        sleep(1)
        Step("设置测试参数test.param.003.................")
        result_set_003 = self.driver.shell('begetctl param set test.param.003 value003')
        Step(result_set_003)
        Step("检查设置参数结果.................")
        self.driver.Assert.contains(result_set_003, 'Set parameter test.param.003 value003 success')
        sleep(1)
        Step("获取测试参数test.param.003.................")
        result_get_003 = self.driver.shell('begetctl param get test.param.003')
        Step(result_get_003)
        Step("检查获取参数结果.................")
        self.driver.Assert.contains(result_get_003, 'value003')

    def teardown(self):
        Step("收尾工作.................")