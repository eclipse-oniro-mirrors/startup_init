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


class SubStartupInitApiname0100(TestCase):

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
        Step("预置工作:检测屏幕是否亮.................")
        self.driver.Screen.wake_up()
        self.driver.ScreenLock.unlock()
        self.driver.Screen.enable_stay_awake()

    def test_step1(self):
        Step("安装测试hap.................")
        sourpath = Common.sourcepath("distributionOsApiName.hap", "sub_startup_init_apiname")
        Step(sourpath)
        self.driver.AppManager.install_app(sourpath)
        Step("命令查看param get const.product.os.dist.apiname.................")
        apiname = self.driver.shell("param get const.product.os.dist.apiname")
        apiname = apiname.strip()
        Step(apiname)
        result = "const.product.os.dist.apiname: " + apiname
        Step(result)
        Step("打开测试hap.................")
        self.driver.start_app("com.example.myapplication", "EntryAbility")
        Step("点击get ApiName按钮.................")
        self.driver.touch(BY.text("get ApiName"))
        sleep(1)
        Step("校验apiname值.................")

    def teardown(self):
        Step("收尾工作.................")
        Step("卸载测试hap................")
        self.driver.AppManager.clear_app_data('com.example.myapplication')
        self.driver.AppManager.uninstall_app('com.example.myapplication')
