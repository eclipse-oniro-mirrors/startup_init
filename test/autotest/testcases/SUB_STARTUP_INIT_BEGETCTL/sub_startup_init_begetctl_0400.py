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


class SunStartupInitBegetctl0400(TestCase):

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
        Step("获取param wait name结果.................")
        result_wait_name = self.driver.shell('begetctl param wait const.cust.vendor')
        Step(result_wait_name)
        Step("检查param wait name结果.................")
        self.driver.Assert.contains(result_wait_name, 'Wait parameter const.cust.vendor success')
        sleep(1)
        Step("获取param wait name value结果.................")
        result = self.driver.shell("begetctl param ls -r const.cust.vendor | grep value")
        result = result.strip()
        value = result.split("value:")[1]
        result_wait_name_value = self.driver.shell('begetctl param wait const.cust.vendor %s' % value)
        Step(result_wait_name_value)
        Step("检查param wait name value结果.................")
        self.driver.Assert.contains(result_wait_name_value, 'Wait parameter const.cust.vendor success')
        sleep(1)
        Step("获取param wait name value timeout结果.................")
        result_wait_name_value_timeout = self.driver.shell('begetctl param wait const.cust.vendor %s 10' % value)
        Step(result_wait_name_value_timeout)
        Step("检查param wait name value timeout结果.................")
        self.driver.Assert.contains(result_wait_name_value_timeout, 'Wait parameter const.cust.vendor success')
        sleep(1)

    def teardown(self):
        Step("收尾工作.................")
