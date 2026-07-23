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


class SunStartupInitBegetctl1900(TestCase):

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
        Step("设置日志级别为DEBUG.................")
        result_debug = self.driver.shell('begetctl set log level 0')
        Step(result_debug)
        Step("检查设置日志级别结果.................")
        self.driver.Assert.contains(result_debug, 'Success to set log level')
        sleep(1)
        Step("设置日志级别为INFO.................")
        result_info = self.driver.shell('begetctl set log level 1')
        Step(result_info)
        Step("检查设置日志级别结果.................")
        self.driver.Assert.contains(result_info, 'Success to set log level INFO')
        sleep(1)
        Step("设置日志级别为WARNING.................")
        result_warning = self.driver.shell('begetctl set log level 2')
        Step(result_warning)
        Step("检查设置日志级别结果.................")
        self.driver.Assert.contains(result_warning, 'Success to set log level')
        sleep(1)
        Step("设置日志级别为ERROR.................")
        result_error = self.driver.shell('begetctl set log level 3')
        Step(result_error)
        Step("检查设置日志级别结果.................")
        self.driver.Assert.contains(result_error, 'Success to set log level')
        sleep(1)
        Step("设置日志级别为FATAL.................")
        result_fatal = self.driver.shell('begetctl set log level 4')
        Step(result_fatal)
        Step("检查设置日志级别结果.................")
        self.driver.Assert.contains(result_fatal, 'Success to set log level')
        sleep(1)
        Step("获取当前日志级别.................")
        result_get = self.driver.shell('begetctl get log level')
        Step(result_get)
        Step("检查获取日志级别结果.................")
        self.driver.Assert.contains(result_get, 'Init log level:')

    def teardown(self):
        Step("收尾工作.................")