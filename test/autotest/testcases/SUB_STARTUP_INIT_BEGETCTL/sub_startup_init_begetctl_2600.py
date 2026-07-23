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


class SunStartupInitBegetctl2600(TestCase):

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
        Step("获取param ls结果.................")
        result_ls = self.driver.shell('begetctl param ls')
        Step(result_ls)
        Step("检查param ls结果.................")
        self.driver.Assert.contains(result_ls, 'Parameter information:')
        sleep(1)
        Step("获取param ls -r结果.................")
        result_ls_r = self.driver.shell('begetctl param ls -r')
        Step(result_ls_r)
        Step("检查param ls -r结果.................")
        self.driver.Assert.contains(result_ls_r, 'Parameter information:')
        self.driver.Assert.contains(result_ls_r, 'name :')
        self.driver.Assert.contains(result_ls_r, 'value:')
        sleep(1)
        Step("获取param ls const.ohos结果.................")
        result_ls_const = self.driver.shell('begetctl param ls const.ohos')
        Step(result_ls_const)
        Step("检查param ls const.ohos结果.................")
        self.driver.Assert.contains(result_ls_const, 'Parameter information:')
        sleep(1)
        Step("获取param ls -r const.ohos结果.................")
        result_ls_r_const = self.driver.shell('begetctl param ls -r const.ohos')
        Step(result_ls_r_const)
        Step("检查param ls -r const.ohos结果.................")
        self.driver.Assert.contains(result_ls_r_const, 'Parameter information:')

    def teardown(self):
        Step("收尾工作.................")