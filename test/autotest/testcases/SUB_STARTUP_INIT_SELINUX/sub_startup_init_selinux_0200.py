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

from devicetest.core.test_case import Step, TestCase
from hypium import *


class SubStartupInitSelinux0200(TestCase):

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

    def test_step1(self):
        Step("查看属性文件映射列表................")
        result = self.driver.shell("ls -lZ /dev/__parameters__/")
        Step(result)
        self.driver.Assert.contains(result, "dev_parameters_file:s0")
        self.driver.Assert.contains(result, "bootevent_param:s0")
        self.driver.Assert.contains(result, "persist_param:s0")
        self.driver.Assert.contains(result, "const_allow_mock_param:s0")

    def teardown(self):
        Step("收尾工作.................")
