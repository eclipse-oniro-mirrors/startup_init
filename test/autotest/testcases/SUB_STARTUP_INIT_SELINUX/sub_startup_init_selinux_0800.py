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
from aw import Common


class SubStartupInitSelinux0800(TestCase):

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
        Step("导入配置文件vendor_init_test.cfg.................")
        self.driver.hdc("target mount")
        sourpath_se = Common.sourcepath("vendor_init_test.cfg", "SUB_STARTUP_INIT_SELINUX")
        destpath_se = "/vendor/etc/init/vendor_init_test.cfg"
        self.driver.push_file(sourpath_se, destpath_se)
        sleep(3)
        Step("重启生效..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()

    def test_step1(self):
        Step("查询系统加载测试trigger.................")
        result = self.driver.shell("begetctl dump_service parameter_service trigger|grep test.randrom.read.start")
        Step(result)
        self.driver.Assert.contains(result, "trigger      name: param:test.randrom.read.start=1")
        self.driver.Assert.contains(result, "trigger condition: test.randrom.read.start=1")

    def teardown(self):
        Step("收尾工作，删除文件................")
        self.driver.shell("rm /vendor/etc/init/vendor_init_test.cfg")
