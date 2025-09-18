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
import time
from devicetest.core.test_case import Step, TestCase
from hypium import *
from hypium.model import UiParam
from aw import Common


class SubStartupInitPropertymgt1400(TestCase):

    def __init__(self, controllers):
        self.TAG = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step("预置工作:初始化PC开始.................")
        self.driver = UiDriver(self.device1)
        Step("预置工作:检测屏幕是否亮.................")
        self.driver.enable_auto_wakeup(self.device1)
        Step("预置工作:滑动解锁.................")
        self.driver.swipe(UiParam.UP, side=UiParam.BOTTOM)
        Step('预置工作:设置屏幕常亮')
        self.driver.Screen.enable_stay_awake()

    def test_step1(self):
        Step("步骤1：执行参数wait")
        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        file = path + '\\testFile\\readme.txt'
        mythread = Common.excmd_async(self.driver, "hdc shell param wait testcase.param.wait ok", file)
        mythread.start()
        time.sleep(10)
        Step("步骤2：设置参数等待ok")
        result2 = self.driver.shell("param set testcase.param.wait ok")
        self.driver.Assert.contains(result2, "Set parameter testcase.param.wait ok success")
        time.sleep(2)
        with open(file, "r") as f:
            result1 = f.read()
        self.driver.Assert.contains(result1, "Wait parameter testcase.param.wait success")

    def teardown(self):
        Step("收尾工作：重启恢复")
        self.driver.shell("reboot")
        self.driver.System.wait_for_boot_complete()
        self.driver.ScreenLock.unlock()
