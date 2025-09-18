#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2025 Huawei Device Co., Ltd.
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

from devicetest.core.test_case import Step, TestCase
from hypium import *

class SUB_STARTUP_INIT_SYSPARAM_ENHANCED_0700(TestCase):

    def __init__(self, controllers):
        self.TAG = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step("预置工作:初始化手机开始.................")
        Step(self.devices[0].device_id)

    def test_step1(self):
        Step("设置系统参数1.................")
        result1 = self.driver.System.execute_command("param set musl.1 1")
        Step(result1)
        Step("设置系统参数2.................")
        result2 = self.driver.System.execute_command("param set musl.2 1")
        Step(result2)
        Step("设置系统参数3.................")
        result3 = self.driver.System.execute_command("param set musl.3 1")
        Step(result3)
        Step("设置系统参数4.................")
        result4 = self.driver.System.execute_command("param set musl.4 1")
        Step(result4)
        Step("设置系统参数5.................")
        result5 = self.driver.System.execute_command("param set musl.5 1")
        Step(result5)
        assert  "Set parameter musl.1 1 fail! errNum is:103!" in result1 or\
                "Set parameter musl.2 1 fail! errNum is:103!" in result2 or\
                "Set parameter musl.3 1 fail! errNum is:103!" in result3 or\
                "Set parameter musl.4 1 fail! errNum is:103!" in result4 or \
                "Set parameter musl.5 1 fail! errNum is:103!" in result5


    def teardown(self):
        Step("收尾工作.................")
