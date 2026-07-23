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


class SunStartupInitBegetctl2300(TestCase):

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
        Step("获取hiview服务的pid.................")
        pid_hiview = self.driver.System.get_pid("hiview")
        Step(pid_hiview)
        sleep(1)
        Step("停止hiview服务.................")
        self.driver.shell('begetctl stop_service hiview')
        sleep(2)
        Step("获取停止后hiview服务的pid.................")
        pid_hiview_stop = self.driver.System.get_pid("hiview")
        Step(pid_hiview_stop)
        sleep(1)
        Step("启动hiview服务.................")
        self.driver.shell('begetctl start_service hiview')
        sleep(2)
        Step("获取启动后hiview服务的pid.................")
        pid_hiview_start = self.driver.System.get_pid("hiview")
        Step(pid_hiview_start)
        sleep(1)
        Step("验证服务重启成功.................")
        if pid_hiview_start:
            Step("hiview服务重启成功.................")

    def teardown(self):
        Step("收尾工作.................")