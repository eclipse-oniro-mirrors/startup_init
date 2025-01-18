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

import time
from devicetest.core.test_case import TestCase, Step
from hypium import UiDriver


class SunStartupInitBase0500(TestCase):
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
        bundle_name = "com.generic.hmos.notepad"
        self.driver.shell("write_updater user_factory_reset")
        self.driver.shell("reboot updater")
        self.driver.System.wait_for_boot_complete()
        self.driver.AppManager.disable_startup_guide()

        wake = self.driver.Screen.is_on()
        time.sleep(0.5)
        if wake:
            self.driver.ScreenLock.unlock()
        else:
            self.driver.Screen.wake_up()
            self.driver.ScreenLock.unlock()
        self.driver.Screen.enable_stay_awake()

        self.driver.AppManager.start_app(bundle_name)
        pid = self.driver.System.get_pid(bundle_name)
        self.driver.Assert.equal(type(pid), int)
        self.driver.AppManager.stop_app(bundle_name)

    def teardown(self):
        Step("收尾工作.................")
