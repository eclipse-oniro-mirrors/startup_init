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

# -*- coding: utf-8 -*-
from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import UiDriver


class SunStartupInitBegetctl1000(TestCase):

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
        Step("获取hiview的pid.................")
        pid = self.driver.System.get_pid("hiview")
        Step(pid)
        sleep(1)
        Step("停止hiview服务.................")
        self.driver.shell('begetctl stop_service hiview')
        Step("获取hiview的pid.................")
        pid1 = self.driver.System.get_pid("hiview")
        Step(pid1)
        sleep(1)
        Step("启动hiview服务.................")
        self.driver.shell('begetctl start_service hiview')
        Step("获取hiview的pid.................")
        pid2 = self.driver.System.get_pid("hiview")
        Step(pid2)
        if (!pid2)


    def teardown(self):
        Step("收尾工作.................")
