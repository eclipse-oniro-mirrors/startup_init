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
from hypium.model import UiParam


class SubStartupInitPid0400(TestCase):

    def __init__(self, controllers):
        self.TAG = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step("预置工作:初始化手机开始.................")
        self.driver = UiDriver(self.device1)
        Step("预置工作:唤醒屏幕.................")
        self.driver.enable_auto_wakeup(self.device1)
        Step("预置工作:滑动解锁.................")
        self.driver.swipe(UiParam.UP, side=UiParam.BOTTOM)

    def test_step1(self):
        Step("检查deviceinfoservice服务.................")
        result = self.driver.shell("param get | grep deviceinfoservice")
        Step(result)
        self.driver.Assert.contains(result, "startup.service.ctl.deviceinfoservice = 2", "deviceinfoservice服务未启动")
        Step("关闭deviceinfoservice服务.................")
        pid = self.driver.System.get_pid("deviceinfoservice")
        self.driver.shell("kill -9 %d" % pid)
        Step("再次检查deviceinfoservice服务.................")
        result = self.driver.shell("param get | grep deviceinfoservice")
        self.driver.Assert.contains(result, "startup.service.ctl.deviceinfoservice = 5", "deviceinfoservice服务未停止")

    def teardown(self):
        Step("收尾工作：重启恢复服务")
        self.driver.shell("reboot")
        self.driver.System.wait_for_boot_complete()
        self.driver.ScreenLock.unlock()
