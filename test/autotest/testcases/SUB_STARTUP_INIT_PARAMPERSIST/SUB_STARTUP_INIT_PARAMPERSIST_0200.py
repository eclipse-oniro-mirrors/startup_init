# Copyright (c) 2024 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import time
from devicetest.core.test_case import TestCase, Step
from hypium import UiDriver

class SUB_STARTUP_INIT_PARAMPERSIST_0200(TestCase):

    def __init__(self, controllers):
        self.Tag = self.__class__.__name__
        TestCase.__init__(self, self.Tag, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step(self.devices[0].device_id)
        device = self.driver.shell("param get const.product.model")
        device = device.replace("\n", "").replace(" ", "")
        device = str(device)
        Step(device)
        # 解锁屏幕
        wake = self.driver.Screen.is_on()
        time.sleep(0.5)
        if wake:
            self.driver.ScreenLock.unlock()
        else:
            self.driver.Screen.wake_up()
            self.driver.ScreenLock.unlock()
        self.driver.Screen.enable_stay_awake()

    def process(self):
        self.driver.hdc("target mount")
        Step("param set设置persist.***参数")
        self.driver.shell("param set persist.test2 false")

        Step("param get获取persist.***参数为false")
        param1 = self.driver.shell("param get persist.test2")
        self.driver.Assert.contains(param1, 'false')

        Step("查询持久化参数文件中有persist.test2=false")
        has_file = self.driver.Storage.has_file("/data/service/el1/startup/parameters/public_persist_parameters")
        self.driver.Assert.equal(has_file, True)
        param2 = self.driver.shell("cat /data/service/el1/startup/parameters/public_persist_parameters").split("\n")
        self.driver.Assert.equal(param2.count("persist.test2=false"), 1)

    def teardown(self):
        Step("收尾工作.................")
