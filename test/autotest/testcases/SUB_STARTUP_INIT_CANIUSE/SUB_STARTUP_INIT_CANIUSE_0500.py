#
# Copyright (c) 2025 Huawei Device Co., Ltd.
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
import time
from devicetest.core.test_case import TestCase, Step
from hypium import UiDriver
from aw import Common


class SubStartupInitCaniuse0500(TestCase):

    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        TestCase.__init__(self, self.tag, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step(self.devices[0].device_id)
        device = self.driver.shell("param get const.product.model")
        device = device.replace("\n", "").replace(" ", "")
        device = str(device)
        Step(device)
        # 解锁屏幕
        wake = self.driver.Screen.is_on()
        time.sleep(0.5)
        if wake:
            self.driver.ScreenLock.unlock()
        else:
            self.driver.Screen.wake_up()
            self.driver.ScreenLock.unlock()
        self.driver.Screen.enable_stay_awake()

    def process(self):
        Step("安装应用")
        text_list = []
        hap_path = Common.sourcepath('caniuse.hap', "SUB_STARTUP_INIT_CANIUSE")
        bundle_name = "com.example.myapplication3"
        hap = self.driver.AppManager.has_app(bundle_name)
        if hap:
            self.driver.AppManager.clear_app_data(bundle_name)
            self.driver.AppManager.uninstall_app(bundle_name)
            self.driver.AppManager.install_app(hap_path)
        else:
            self.driver.AppManager.install_app(hap_path)
        self.driver.AppManager.start_app(bundle_name)

        comps1 = self.driver.find_all_components(BY.type("Select"))
        self.driver.touch(comps1[0])
        comps2 = self.driver.find_all_components(BY.type("Option"))
        self.driver.touch(comps2[1])
        comps1 = self.driver.find_all_components(BY.type("Select"))
        self.driver.touch(comps1[1])
        comps3 = self.driver.find_all_components(BY.type("Option"))
        self.driver.touch(comps3[1])
        self.driver.touch(BY.text("click me"))
        time.sleep(1)
        comp4 = self.driver.find_all_components(BY.type("Text"))
        for text in comp4:
            text_list.append(text.getText())
        self.driver.Assert.contains(text_list, "can use SystemCapability.Location.Location.Core in task_0 task_1 task_2 , There are 3 tasks in total")

    def teardown(self):
        Step("收尾工作.................")
        self.driver.AppManager.clear_app_data(bundle_name)
        self.driver.AppManager.uninstall_app(bundle_name)
