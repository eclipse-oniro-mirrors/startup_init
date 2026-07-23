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


class SunStartupInitBegetctl3600(TestCase):

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
        Step("获取const.product.devicetype参数.................")
        result_device = self.driver.shell('begetctl param get const.product.devicetype')
        Step(result_device)
        Step("检查参数结果.................")
        self.driver.Assert.contains(result_device, 'default')
        sleep(1)
        Step("获取const.product.manufacturer参数.................")
        result_manu = self.driver.shell('begetctl param get const.product.manufacturer')
        Step(result_manu)
        Step("检查参数结果.................")
        sleep(1)
        Step("获取const.product.brand参数.................")
        result_brand = self.driver.shell('begetctl param get const.product.brand')
        Step(result_brand)
        Step("检查参数结果.................")
        sleep(1)
        Step("获取const.product.name参数.................")
        result_name = self.driver.shell('begetctl param get const.product.name')
        Step(result_name)
        Step("检查参数结果.................")
        sleep(1)
        Step("获取const.product.model参数.................")
        result_model = self.driver.shell('begetctl param get const.product.model')
        Step(result_model)
        Step("检查参数结果.................")
        sleep(1)
        Step("获取const.build.version参数.................")
        result_version = self.driver.shell('begetctl param get const.build.version')
        Step(result_version)
        Step("检查参数结果.................")

    def teardown(self):
        Step("收尾工作.................")