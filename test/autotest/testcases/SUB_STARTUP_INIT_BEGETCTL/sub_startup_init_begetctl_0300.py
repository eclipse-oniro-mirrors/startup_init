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


class SunStartupInitBegetctl0300(TestCase):

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
        Step("设置参数.................")
        result_set = self.driver.shell('begetctl param set test test.value')
        Step(result_set)
        Step("检查设置参数.................")
        self.driver.Assert.contains(result_set, 'Set parameter test test.value success')
        sleep(1)
        Step("获取参数.................")
        result_get = self.driver.shell('begetctl param get test')
        Step(result_get)
        Step("检查获取参数.................")
        self.driver.Assert.contains(result_get, 'test.value')

    def teardown(self):
        Step("收尾工作.................")
