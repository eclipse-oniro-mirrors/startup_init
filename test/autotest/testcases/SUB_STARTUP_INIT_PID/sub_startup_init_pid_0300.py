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


class SubStartupInitPid0300(TestCase):

    def __init__(self, controllers):
        self.TAG = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step("预置工作:初始化手机开始.................")
        Step(self.devices[0].device_id)
        self.driver.shell("param set deviceinfoservice = 5")

    def test_step1(self):
        self.driver.shell("begetctl dump api")
        sleep(3)
        Step("检查deviceinfoservice服务.................")
        result = self.driver.shell("param get | grep deviceinfoservice")
        Step(result)
        self.driver.Assert.contains(result, "startup.service.ctl.deviceinfoservice = 2", "deviceinfoservice服务未启动")

    def teardown(self):
        Step("收尾工作.................")
