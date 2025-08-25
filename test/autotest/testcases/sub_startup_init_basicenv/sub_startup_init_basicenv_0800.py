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


class SubStartupInitBasicenv0800(TestCase):

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

    def test_step1(self):
        Step("find cmdline................")
        result_cmdline = self.driver.shell("cat /proc/cmdline")
        Step(result_cmdline)

        Step("识别设备型号...............................")
        device = self.driver.shell("param get const.product.model")
        device = device.replace("\n", "").replace(" ", "")
        device = str(device)
        if ("ALN" in device):
            self.driver.Assert.contains(result_cmdline, 'hardware=kirin9000s', "ERROR: cannot find hardware in /proc/cmdline.")
            Step("param get hardware................")
            result_hardware = self.driver.shell("param get ohos.boot.hardware")
            Step(result_hardware)
            self.driver.Assert.contains(result_hardware, 'kirin9000s', "ERROR: cannot find hardware in param get.")
        elif ("CLS" in device):
            self.driver.Assert.contains(result_cmdline, 'hardware=Kirin9010', "ERROR: cannot find hardware in /proc/cmdline.")
            result_hardware = self.driver.shell("param get ohos.boot.hardware")
            Step(result_hardware)
            self.driver.Assert.contains(result_hardware, 'Kirin9010', "ERROR: cannot find hardware in param get.")
        else:
            self.driver.Assert.contains(result_cmdline, 'hardware=kirin9020', "ERROR: cannot find hardware in /proc/cmdline.")
            Step("param get hardware................")
            result_hardware = self.driver.shell("param get ohos.boot.hardware")
            Step(result_hardware)
            self.driver.Assert.contains(result_hardware, 'kirin9020', "ERROR: cannot find hardware in param get.")

    def teardown(self):
        Step("收尾工作.................")
