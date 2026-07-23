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


class SunStartupInitBegetctl3100(TestCase):

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
        Step("设置persist类型参数persist.test.param.001.................")
        result_set_1 = self.driver.shell('begetctl param set persist.test.param.001 persist_value_001')
        Step(result_set_1)
        Step("检查设置参数结果.................")
        self.driver.Assert.contains(result_set_1, 'Set parameter persist.test.param.001 persist_value_001 success')
        sleep(1)
        Step("获取persist类型参数persist.test.param.001.................")
        result_get_1 = self.driver.shell('begetctl param get persist.test.param.001')
        Step(result_get_1)
        Step("检查获取参数结果.................")
        self.driver.Assert.contains(result_get_1, 'persist_value_001')
        sleep(1)
        Step("设置persist类型参数persist.test.param.002.................")
        result_set_2 = self.driver.shell('begetctl param set persist.test.param.002 persist_value_002')
        Step(result_set_2)
        Step("检查设置参数结果.................")
        self.driver.Assert.contains(result_set_2, 'Set parameter persist.test.param.002 persist_value_002 success')
        sleep(1)
        Step("获取persist类型参数persist.test.param.002.................")
        result_get_2 = self.driver.shell('begetctl param get persist.test.param.002')
        Step(result_get_2)
        Step("检查获取参数结果.................")
        self.driver.Assert.contains(result_get_2, 'persist_value_002')
        sleep(1)
        Step("保存持久化参数.................")
        result_save = self.driver.shell('begetctl param save')
        Step(result_save)
        Step("检查保存参数结果.................")
        self.driver.Assert.contains(result_save, 'Save persist parameters success')

    def teardown(self):
        Step("收尾工作.................")