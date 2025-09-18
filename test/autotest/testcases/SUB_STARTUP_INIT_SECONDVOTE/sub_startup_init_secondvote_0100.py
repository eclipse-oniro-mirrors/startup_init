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


class SubStartupInitSecondvote0100(TestCase):

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
        Step("清理hilog目录")
        self.driver.shell("rm -rf /data/log/hilog/*")

        Step("杀掉foundation进程后，新pid与老pid不同 断言")
        result = self.driver.shell('ps -ef | grep foundation | grep -v grep')
        foundation_pid1 = result.split()[1]
        self.driver.shell("kill -15 %d" % int(foundation_pid1))
        time.sleep(3)
        result = self.driver.shell('ps -ef | grep foundation | grep -v grep')
        foundation_pid2 = result.split()[1]
        for i in [foundation_pid1, foundation_pid2]:
            self.driver.Assert.equal(i.isdigit(), True)
        self.driver.Assert.not_equal(foundation_pid1, foundation_pid2)

        Step("拉取日志，匹配关键词")
        self.driver.System.wait_for_boot_complete()
        result = self.driver.shell('ls /data/log/hilog').split()
        count = 0
        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        targetpath = os.path.join(path, "testFile", "Log")
        for file in result:
            if 'kmsg' in file:
                self.driver.pull_file('/data/log/hilog/%s' % file, '%s' % targetpath)
                fileabspath = os.path.join(targetpath, file)
                with gzip.open(fileabspath, 'rb') as f:
                    lines = f.readlines()
                    for line in lines:
                        if b'All boot events are fired, boot complete now' in line:
                            count += 1
        self.driver.Assert.greater_equal(count, 1)

    def teardown(self):
        Step("收尾工作.................")
        self.driver.System.reboot()
        self.driver.System.wait_for_boot_complete()

