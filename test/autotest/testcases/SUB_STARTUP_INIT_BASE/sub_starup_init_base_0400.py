# -*- coding: utf-8 -*-

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

import os
import sys
from devicetest.core.test_case import TestCase, Step
from hypium import UiDriver
from aw import Common


class SunStartupInitBase0400(TestCase):
    def __init__(self, controllers):
        self.tag = self.__class__.__name__
        TestCase.__init__(self, self.tag, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step(self.devices[0].device_id)
        device = self.driver.shell("param get const.product.model")
        device = device.replace("\n", "").replace(" ", "")
        device = str(device)
        Step(device)

    def process(self):
        result = self.driver.hdc("target mount")
        self.driver.Assert.contains(result, "Mount finish")

        txt_path = Common.sourcepath('test.txt', "SUB_STARTUP_INIT_BASE")
        self.driver.push_file(txt_path, "/system")

        has_file = self.driver.Storage.has_file("/system/test.txt")
        self.driver.Assert.equal(has_file, True)
        self.driver.shell("echo '123456' > /system/test.txt")

        self.driver.System.reboot()
        self.driver.System.wait_for_boot_complete()

        content = self.driver.shell("cat /system/test.txt")
        self.driver.Assert.contains(content, "123456")

    def teardown(self):
        Step("收尾工作.................")
