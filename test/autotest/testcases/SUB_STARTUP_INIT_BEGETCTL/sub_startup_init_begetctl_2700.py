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


class SunStartupInitBegetctl2700(TestCase):

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
        Step("执行param wait const.product.devicetype.................")
        result_wait_1 = self.driver.shell('begetctl param wait const.product.devicetype')
        Step(result_wait_1)
        Step("检查param wait结果.................")
        self.driver.Assert.contains(result_wait_1, 'Wait parameter const.product.devicetype success')
        sleep(1)
        Step("执行param wait const.product.manufacturer.................")
        result_wait_2 = self.driver.shell('begetctl param wait const.product.manufacturer')
        Step(result_wait_2)
        Step("检查param wait结果.................")
        self.driver.Assert.contains(result_wait_2, 'Wait parameter const.product.manufacturer success')
        sleep(1)
        Step("执行param wait const.product.brand.................")
        result_wait_3 = self.driver.shell('begetctl param wait const.product.brand')
        Step(result_wait_3)
        Step("检查param wait结果.................")
        self.driver.Assert.contains(result_wait_3, 'Wait parameter const.product.brand success')
        sleep(1)
        Step("执行param wait const.build.characteristics.................")
        result_wait_4 = self.driver.shell('begetctl param wait const.build.characteristics')
        Step(result_wait_4)
        Step("检查param wait结果.................")
        self.driver.Assert.contains(result_wait_4, 'Wait parameter const.build.characteristics success')

    def teardown(self):
        Step("收尾工作.................")