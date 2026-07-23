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


class SunStartupInitBegetctl2400(TestCase):

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
        Step("执行service_control stop hiview.................")
        result_stop = self.driver.shell('begetctl service_control stop hiview')
        Step(result_stop)
        sleep(2)
        Step("检查hiview服务状态.................")
        pid_stop = self.driver.System.get_pid("hiview")
        Step(pid_stop)
        sleep(1)
        Step("执行service_control start hiview.................")
        result_start = self.driver.shell('begetctl service_control start hiview')
        Step(result_start)
        sleep(2)
        Step("检查hiview服务状态.................")
        pid_start = self.driver.System.get_pid("hiview")
        Step(pid_start)
        sleep(1)
        Step("执行service_control restart hiview.................")
        result_restart = self.driver.shell('begetctl service_control restart hiview')
        Step(result_restart)
        sleep(2)
        Step("检查hiview服务状态.................")
        pid_restart = self.driver.System.get_pid("hiview")
        Step(pid_restart)

    def teardown(self):
        Step("收尾工作.................")