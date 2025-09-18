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

import time
from devicetest.core.test_case import TestCase, Step
from hypium import *
from aw import Common

class SUB_STARTUP_INIT_SERIAL_0200(TestCase):

    def __init__(self, controllers):
        self.TAG = self.__class__.__name__
        TestCase.__init__(self, self.TAG, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step(self.devices[0].device_id)
        global device
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
        self.driver.shell("power-shell setmode 602")

    def process(self):
        global deviceinfo_list
        hap_path = Common.sourcepath('System.hap', "SUB_STARTUP_INIT_SERIAL")
        bundle_name = "com.example.system"
        hap = self.driver.AppManager.has_app(bundle_name)
        if hap:
            self.driver.AppManager.clear_app_data(bundle_name)
            self.driver.AppManager.uninstall_app(bundle_name)
            self.driver.AppManager.install_app(hap_path)
        else:
            self.driver.AppManager.install_app(hap_path)
        self.driver.AppManager.start_app(bundle_name)
        comps1 = self.driver.find_all_components(BY.type("Button"))
        self.driver.touch(comps1[0])
        comps2 = self.driver.find_all_components(BY.type("Text"))
        for text in comps2:
            if text.getText().startswith("device info:"):
                deviceinfo_list = text.getText().split('\n')
        for param in ['  serial: ', '  udid: ']:
            for deviceinfo in deviceinfo_list:
                if deviceinfo.startswith(param):
                    self.driver.Assert.not_equal(deviceinfo.split(":")[1], "")

    def teardown(self):
        Step("收尾工作.................")
        self.driver.AppManager.uninstall_app("com.example.system")