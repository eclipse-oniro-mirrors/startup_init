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
import gzip
from devicetest.core.test_case import TestCase, Step
from hypium import *

class SUB_STARTUP_INIT_STATISTICS_0100(TestCase):

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


    def process(self):
        Step("开启debug日志")
        self.driver.shell("param set persist.init.debug.loglevel 0")

        Step("重启手机")
        self.driver.System.reboot()
        self.driver.System.wait_for_boot_complete()

        Step("解锁屏幕")
        wake = self.driver.Screen.is_on()
        time.sleep(0.5)
        if wake:
            self.driver.ScreenLock.unlock()
        else:
            self.driver.Screen.wake_up()
            self.driver.ScreenLock.unlock()
        self.driver.shell("power-shell timeout -o 86400000")

        Step("拉取日志匹配关键字")
        result = self.driver.shell('ls /data/log/hilog').split()
        count1, count2 = 0, 0
        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        log = os.path.join(path, "testFile")
        for file in result:
            if 'kmsg' in file:
                self.driver.hdc('file recv /data/log/hilog/%s %s' % (file, log))
                with gzip.open(os.path.join(log, file), 'rb') as f:
                    lines = f.readlines()
                    for line in lines:
                        if b'took' in line:
                            count1 += 1
                        if b'Service info' in line:
                            count2 += 1
        self.driver.Assert.greater_equal(count1, 1)
        self.driver.Assert.greater_equal(count2, 1)

    def teardown(self):
        Step("收尾工作.................")

