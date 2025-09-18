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


class SubStartupInitParampersist0300(TestCase):

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
        self.driver.Screen.enable_stay_awake()

    def process(self):
        self.driver.hdc("target mount")
        Step("hilog.para文件中添加persist.***")
        self.driver.shell("sed -e '16a\persist.test3=true' system/etc/param/hilog.para")

        Step("重启设备")
        self.driver.System.reboot()
        self.driver.System.wait_for_boot_complete()

        Step("确认持久化参数文件中无persist.***")
        param1 = self.driver.shell("cat /data/service/el1/startup/parameters/persist_parameters").split("\n")
        self.driver.Assert.equal(param1.count("persist.test3=false"), 0)

        time.sleep(3)
        Step("param set设置persist.***参数后查询")
        self.driver.shell("param set persist.test3 false")
        has_file = self.driver.Storage.has_file("/data/service/el1/startup/parameters/public_persist_parameters")
        self.driver.Assert.equal(has_file, True)
        param2 = self.driver.shell("cat /data/service/el1/startup/parameters/public_persist_parameters").split("\n")
        self.driver.Assert.equal(param2.count("persist.test3=false"), 1)

    def teardown(self):
        Step("收尾工作.................")
