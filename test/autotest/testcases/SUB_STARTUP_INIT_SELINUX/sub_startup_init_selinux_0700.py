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


class SubStartupInitSelinux0700(TestCase):

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
        sourpath = Common.sourcepath("vendor_init_test.cfg", "SUB_STARTUP_INIT_SELINUX")
        destpath = "/vendor/etc/init/vendor_init_test.cfg"
        self.driver.push_file(sourpath, destpath)
        sleep(3)
        Step("重启生效..........................")
        self.driver.System.reboot()
        sleep(3)
        self.driver.System.wait_for_boot_complete()

    def test_step1(self):
        Step("设置系统参数.................")
        self.driver.shell('param set test.randrom.read.start 1')
        sleep(3)
        Step("立即查询init进程.................")
        result = self.driver.shell("ps -efZ|grep init")
        Step(result)
        self.driver.Assert.contains(result, "chipset_init", "chipset vendor_init未启动2")
        Step("30s后查询init进程.................")
        sleep(30)
        result30 = self.driver.shell("ps -efZ|grep init")
        Step(result30)
        assert 'chipset_init' not in result30

    def teardown(self):
        Step("收尾工作.................")
        Step("收尾工作，删除文件................")
        self.driver.shell("rm /vendor/etc/init/vendor_init_test.cfg")
