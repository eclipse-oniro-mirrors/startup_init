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

from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import *


class SUB_STARTUP_INIT_STARTSTOP_1300(TestCase):

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
        Step("初始查看deviceinfo服务是否存在..........................")
        pid1 = self.driver.System.get_pid('deviceinfoservice')
        Step(pid1)
        if pid1 :
            Step("初始deviceinfo进程存在")
            Step("kill初始进程")
            self.driver.shell("kill -9 " + str(pid1))
        else:
            Step("初始deviceinfo进程不存在")
            pass
        Step("执行dump api，查看服务是否存在..........................")
        self.driver.shell('begetctl dump api')
        pid2 = self.driver.System.get_pid('deviceinfoservice')
        Step(pid2)
        assert pid2
        Step("等待90s，查看deviceinfo服务是否存在..........................")
        sleep(90)
        hidumper = self.driver.shell('hidumper -s 0 -a "-l"|grep 3902 -C 1')
        hidumper = hidumper.split('\n')[2]
        hidumper = hidumper.split(':')[1]
        Step(hidumper)
        self.driver.Assert.contains(hidumper, 'UNLOADABLE')

    def teardown(self):
        Step("收尾工作.................")
