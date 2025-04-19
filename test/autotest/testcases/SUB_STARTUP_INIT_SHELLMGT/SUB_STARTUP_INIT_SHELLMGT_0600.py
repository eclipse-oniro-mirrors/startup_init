#
# Copyright (c) 2025 Huawei Device Co., Ltd.
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
from devicetest.core.test_case import Step, TestCase
from hypium import UiDriver


class SubStartupInitShellmgt0600(TestCase):

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
        Step("查看条件..........................")
        condition = self.driver.shell('cat /etc/init.cfg|grep chmod')
        Step(condition)
        Step("条件包括/dev/access_token_id..........................")
        self.driver.Assert.contains(condition, 'chmod 0666 /dev/access_token_id')
        Step("校验条件是否生效..........................")
        condition = self.driver.shell('ls -l /dev/access_token_id')
        Step(condition)
        self.driver.Assert.contains(condition, 'crw-rw-rw-')

    def teardown(self):
        Step("收尾工作.................")
