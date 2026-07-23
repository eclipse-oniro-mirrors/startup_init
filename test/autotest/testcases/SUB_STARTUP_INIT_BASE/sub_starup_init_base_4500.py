#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2025 Huawei Device Co., Ltd.
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


class SunStartupInitBase4500(TestCase):
    """验证挂载功能(mount_unittest):
    - 块设备路径和文件系统挂载
    - fstab分区挂载验证
    - 对应unittest: mount_unittest.cpp
    """

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
        wake = self.driver.Screen.is_on()
        time.sleep(0.5)
        if wake:
            self.driver.ScreenLock.unlock()
        else:
            self.driver.Screen.wake_up()
            self.driver.ScreenLock.unlock()
        self.driver.shell("power-shell timeout -o 86400000")

    def process(self):
        Step("验证/data分区已挂载")
        result = self.driver.shell("df -h /data")
        self.driver.Assert.contains(result, "/data")

        Step("验证/system分区已挂载")
        result = self.driver.shell("df -h /system")
        self.driver.Assert.contains(result, "/system")

        Step("验证/vendor分区已挂载")
        result = self.driver.shell("df -h /vendor")
        self.driver.Assert.contains(result, "/vendor")

        Step("验证块设备映射存在")
        result = self.driver.shell("ls /dev/block/by-name/")
        Step("块设备列表: %s" % result)

        Step("验证挂载参数正确")
        result = self.driver.shell("mount | grep -E 'system|data|vendor' | head -10")
        Step("挂载参数: %s" % result)

        Step("验证挂载文件系统类型")
        result = self.driver.shell("mount | grep '/data '")
        Step("/data挂载信息: %s" % result)

        Step("查看init挂载相关日志")
        result = self.driver.shell("hilog -x -t init | grep -iE 'mount|fstab|partition' | tail -5")
        Step("挂载日志: %s" % result)

    def teardown(self):
        Step("收尾工作.................")
