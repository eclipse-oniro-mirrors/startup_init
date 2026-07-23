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


class SunStartupInitBegetctl2000(TestCase):

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
        Step("获取init进程的pid.................")
        pid_init = self.driver.System.get_pid("init")
        Step(pid_init)
        sleep(1)
        Step("查看init进程的memory.................")
        result_init = self.driver.shell('begetctl display memory ' + str(pid_init))
        Step(result_init)
        Step("检查结果.................")
        self.driver.Assert.contains(result_init, 'Total mem')
        sleep(1)
        Step("获取appspawn进程的pid.................")
        pid_appspawn = self.driver.System.get_pid("appspawn")
        Step(pid_appspawn)
        sleep(1)
        Step("查看appspawn进程的memory.................")
        result_appspawn = self.driver.shell('begetctl display memory ' + str(pid_appspawn))
        Step(result_appspawn)
        Step("检查结果.................")
        self.driver.Assert.contains(result_appspawn, 'Total mem')
        sleep(1)
        Step("获取sampler_proc进程的pid.................")
        pid_sampler = self.driver.System.get_pid("sampler_proc")
        Step(pid_sampler)
        if pid_sampler:
            Step("查看sampler_proc进程的memory.................")
            result_sampler = self.driver.shell('begetctl display memory ' + str(pid_sampler))
            Step(result_sampler)
            Step("检查结果.................")
            self.driver.Assert.contains(result_sampler, 'Total mem')

    def teardown(self):
        Step("收尾工作.................")