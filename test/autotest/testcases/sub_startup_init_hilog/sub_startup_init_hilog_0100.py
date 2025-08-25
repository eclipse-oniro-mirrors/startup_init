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

import os
from devicetest.core.test_case import TestCase, Step, CheckPoint
from devicetest.core.test_case import Step, TestCase
from hypium import UiDriver
from hypium.action.os_hypium.device_logger import DeviceLogger
from hypium.model import UiParam, WindowFilter
import time


class SubStartupInitHilog0100(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.tag, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step("预置工作:初始化手机开始.................")
        self.driver = UiDriver(self.device1)
        Step("预置工作:检测屏幕是否亮.................")
        self.driver.enable_auto_wakeup(self.device1)
        Step("预置工作:滑动解锁.................")
        self.driver.swipe(UiParam.UP, side=UiParam.BOTTOM)
        Step('设置屏幕常亮')
        self.driver.Screen.enable_stay_awake()

    def test_step1(self):
        Step("步骤1：过滤关键日志")
        device_logger = DeviceLogger(self.driver)
        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        device_logger.set_filter_string("C02C11")
        Step("步骤2：获取appspawn的进程号")
        pid = self.driver.System.get_pid("appspawn")
        Step("步骤3：开始抓日志")
        device_logger.start_log(path + '\\testFile\\log\\%s.log' % (self.TAG))
        Step("步骤4：kill进程号")
        self.driver.shell("kill -9 %d" % pid)
        time.sleep(5)
        Step("步骤4：关闭日志")
        device_logger.stop_log()
        time.sleep(2)
        device_logger.check_not_exist_keyword('nodict')

    def teardown(self):
        Step("收尾工作:无")