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


class SunStartupInitBase3600(TestCase):

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
        Step("执行命令begetctl..........................")
        result_beg = self.driver.shell('begetctl')
        Step(result_beg)
        self.driver.Assert.contains(result_beg, 'dump_service loop')
        Step("执行命令begetctl dump_service..........................")
        result_beg_dump_ser = self.driver.shell('begetctl dump_service')
        Step(result_beg_dump_ser)
        self.driver.Assert.contains(result_beg_dump_ser, 'dump_service loop')
        Step("执行命令begetctl dump_service loop..........................")
        result_beg_dump_ser_loop = self.driver.shell('begetctl dump_service loop')
        Step(result_beg_dump_ser_loop)
        self.driver.Assert.contains(result_beg_dump_ser_loop, 'dump service info')

    def teardown(self):
        Step("收尾工作.................")
