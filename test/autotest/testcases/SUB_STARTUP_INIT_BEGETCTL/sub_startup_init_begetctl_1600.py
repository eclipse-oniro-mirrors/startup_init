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


class SunStartupInitBegetctl1600(TestCase):

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
        Step("执行begetctl帮助命令.................")
        result_help = self.driver.shell('begetctl --help')
        Step(result_help)
        Step("检查帮助命令结果.................")
        self.driver.Assert.contains(result_help, 'Usage')
        sleep(1)
        Step("执行begetctl -h命令.................")
        result_h = self.driver.shell('begetctl -h')
        Step(result_h)
        Step("检查-h命令结果.................")
        self.driver.Assert.contains(result_h, 'Usage')
        sleep(1)
        Step("执行begetctl help命令.................")
        result_help_cmd = self.driver.shell('begetctl help')
        Step(result_help_cmd)
        Step("检查help命令结果.................")
        self.driver.Assert.contains(result_help_cmd, 'Usage')
        sleep(1)
        Step("执行begetctl无参数命令.................")
        result_no_param = self.driver.shell('begetctl')
        Step(result_no_param)
        Step("检查无参数命令结果.................")
        self.driver.Assert.contains(result_no_param, 'Usage')

    def teardown(self):
        Step("收尾工作.................")