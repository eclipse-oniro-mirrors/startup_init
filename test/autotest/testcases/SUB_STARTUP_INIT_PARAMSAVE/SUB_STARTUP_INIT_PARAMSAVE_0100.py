# Copyright (c) 2024 Huawei Device Co., Ltd.
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

# -*- coding: utf-8 -*-
from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import *


class SUB_STARTUP_INIT_PARAMSAVE_0100(TestCase):

    def __init__(self, controllers):
        self.Tag = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.Tag, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step("预置工作:初始化手机开始.................")
        Step(self.devices[0].device_id)

    def test_step1(self):
        Step("第一次设置参数persist.test................")
        persist_test_1 = self.driver.shell("param set persist.test 123")
        Step(persist_test_1)
        self.driver.Assert.contains(persist_test_1,'Set parameter persist.test 123 success', '参数设置失败')
        sleep(1)
        Step("检查设置结果................")
        persist_test_result_1 = self.driver.shell(
            "cat /data/service/el1/startup/parameters/public_persist_parameters | grep persist.test")
        self.driver.Assert.contains(persist_test_result_1, "persist.test=123", '持久化参数设置失败')
        Step("第二次设置参数persist.test................")
        persist_test_2 = self.driver.shell("param set persist.test 123456")
        Step(persist_test_2)
        self.driver.Assert.contains(persist_test_2, 'Set parameter persist.test 123456 success', '参数设置失败')
        Step("第三次设置参数persist.test................")
        persist_test_3 = self.driver.shell("param set persist.test abc123")
        Step(persist_test_3)
        self.driver.Assert.contains(persist_test_3, 'Set parameter persist.test abc123 success', '参数设置失败')
        Step('保存参数设置')
        persist_test_save= self.driver.shell("param save")
        Step(persist_test_save)
        self.driver.Assert.contains(persist_test_save, 'Save persist parameters success', '参数保存失败')
        Step("检查保存结果................")
        persist_test_result = self.driver.shell(
            "cat /data/service/el1/startup/parameters/public_persist_parameters| grep persist.test")
        self.driver.Assert.contains(persist_test_result, "persist.test=abc123", '持久化参数保存失败')

    def teardown(self):
        Step("收尾工作.................")
