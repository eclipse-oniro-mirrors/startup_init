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

from time import sleep
from devicetest.core.test_case import Step, TestCase
from hypium import *


class SUB_STARTUP_INIT_STATISTICS_0200(TestCase):

    def __init__(self, controllers):
        self.TAG = self.__class__.__name__
        self.tests = [
            "test_step1"
        ]
        TestCase.__init__(self, self.TAG, controllers)
        self.driver = UiDriver(self.device1)

    def setup(self):
        Step("预置工作:初始化手机开始.................")
        Step(self.devices[0].device_id)

    def test_step1(self):
        Step("获取boot.time.................")
        boot_time_result = self.driver.shell('param get|grep boot.time')
        Step(boot_time_result)
        Step("检查boot.time结果.................")
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.kernel')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.wms.fullscreen.ready')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.wms.ready')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.samgr.ready')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.boot.completed')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.param_watcher.started')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.appspawn.started')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.useriam.fwkready')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.appfwk.ready')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.account.ready')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.lockscreen.ready')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.bootanimation.ready')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.bootanimation.started')
        self.driver.Assert.contains(boot_time_result, 'ohos.boot.time.bootanimation.finished')
        Step("获取bootevent.................")
        bootevent_result = self.driver.shell('param get|grep bootevent')
        Step(bootevent_result)
        Step("检查boot.time结果.................")
        self.driver.Assert.contains(bootevent_result, 'bootevent.account.ready = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.appfwk.ready = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.samgr.ready = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.bootanimation.ready = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.bootanimation.started = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.bootanimation.finished = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.param_watcher.started = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.appspawn.started = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.lockscreen.ready = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.boot.completed = true')
        self.driver.Assert.contains(bootevent_result, 'persist.init.bootevent.enable = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.wms.fullscreen.ready = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.wms.ready = true')
        self.driver.Assert.contains(bootevent_result, 'bootevent.useriam.fwkready = true')


    def teardown(self):
        Step("收尾工作.................")
