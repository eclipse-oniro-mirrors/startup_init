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
from hypium import UiDriver

from aw import Common


class SubStartupInitOdid0100(TestCase):

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
        Step("安装测试hap.................")
        sourpath = Common.sourcepath("odid.hap", "sub_startup_init_odid")
        Step(sourpath)
        self.driver.AppManager.install_app(sourpath)
        Step("预置工作:检测屏幕是否亮.................")
        self.driver.Screen.wake_up()
        self.driver.ScreenLock.unlock()
        self.driver.Screen.enable_stay_awake()

    def test_step1(self):
        Step("打开测试hap.................")
        self.driver.start_app("com.example.newodidtest", "EntryAbility")
        sleep(3)
        Step("点击菜单.................")
        self.driver.touch(BY.text("dev2 ODID1 "))
        sleep(1)
        Step("获取首次odid.................")
        component = self.driver.find_component(BY.type("Text"))
        odid1 = component.getText()
        Step(odid1)
        Step("卸载测试hap................")
        self.driver.AppManager.clear_app_data('com.example.newodidtest')
        self.driver.AppManager.uninstall_app('com.example.newodidtest')
        sleep(3)
        Step("再次安装hap.................")
        sourpath = Common.sourcepath("odid.hap", "sub_startup_init_odid")
        Step(sourpath)
        self.driver.AppManager.install_app(sourpath)
        Step("打开测试hap.................")
        self.driver.start_app("com.example.newodidtest", "EntryAbility")
        sleep(3)
        Step("点击菜单.................")
        self.driver.touch(BY.text("dev2 ODID1 "))
        sleep(1)
        Step("获取二次odid.................")
        component = self.driver.find_component(BY.type("Text"))
        odid2 = component.getText()
        Step(odid2)

    def teardown(self):
        Step("收尾工作.................")
        Step("卸载测试hap................")
        self.driver.AppManager.clear_app_data('com.example.newodidtest')
        self.driver.AppManager.uninstall_app('com.example.newodidtest')
