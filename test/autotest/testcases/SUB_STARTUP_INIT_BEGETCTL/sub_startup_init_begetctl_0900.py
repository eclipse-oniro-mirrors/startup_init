#
# Copyright (c) 2024 Huawei Device Co., Ltd.
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

import gzip
import os
# -*- coding: utf-8 -*-
from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import UiDriver


class SunStartupInitBegetctl0900(TestCase):

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
        Step("执行begetctl命令")
        service_list = ["begetctl timer_start media_service 10000", "begetctl stop_service media_service", "dmesg|grep media_service"]
        for i in service_list:
            self.driver.shell(i)
            sleep(3)

        Step("日志匹配")
        result = self.driver.shell('ls /data/log/hilog').split()
        count = 0
        path = os.path.abspath(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        log = os.path.join(path, "testFile", "Log")
        self.driver.Assert.greater(count, 0)

    def teardown(self):
        Step("收尾工作.................")
