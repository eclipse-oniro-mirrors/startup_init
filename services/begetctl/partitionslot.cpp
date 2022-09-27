/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>

#include "begetctl.h"
#include "hdi/idevmgr_hdi.h"
#include "v1_0/ipartition_slot.h"

using namespace OHOS::HDI::Partitionslot::V1_0;
using OHOS::HDI::DeviceManager::V1_0::IDeviceManager;
static const int32_t PARTITION_ARGC = 2;

static int LoadDevice()
{
    auto devmgr = IDeviceManager::Get();
    if (devmgr != nullptr) {
        return devmgr->LoadDevice("partition_slot_service");
    } else {
        std::cout << "Get devmgr failed" << std::endl;
        return -1;
    }
}

static void UnloadDevice()
{
    auto devmgr = IDeviceManager::Get();
    if (devmgr != nullptr) {
        devmgr->UnloadDevice("partition_slot_service");
    } else {
        std::cout << "Get devmgr failed" << std::endl;
    }
}

static int GetSlot(BShellHandle handle, int32_t argc, char *argv[])
{
    if (LoadDevice() != 0) {
        std::cout << "Load partitionslot device failed" << std::endl;
        return -1;
    }
    std::cout << "Command: partitionslot getslot" << std::endl;
    sptr<IPartitionSlot> partitionslot = IPartitionSlot::Get();
    int bootSlots = 0;
    int currentSlot = 0;
    if (partitionslot == nullptr) {
        std::cout << "Get partitionslot failed" << std::endl;
        return -1;
    }
    partitionslot->GetCurrentSlot(currentSlot, bootSlots);
    std::cout << "The number of slots: " << bootSlots << "," <<  "current slot: " << currentSlot << std::endl;
    UnloadDevice();
    return 0;
}

static int GetSuffix(BShellHandle handle, int32_t argc, char *argv[])
{
    if (argc != PARTITION_ARGC) {
        BShellCmdHelp(handle, argc, argv);
        return -1;
    }
    if (LoadDevice() != 0) {
        std::cout << "Load partitionslot device failed" << std::endl;
        return -1;
    }
    std::cout << "Command: partitionslot getsuffix" << std::endl;
    int slot = atoi(argv[1]);
    std::string suffix = "";
    sptr<IPartitionSlot> partitionslot = IPartitionSlot::Get();
    if (partitionslot == nullptr) {
        std::cout << "Get partitionslot failed" << std::endl;
        return -1;
    }
    partitionslot->GetSlotSuffix(slot, suffix);
    std::cout << "The slot " << slot << " matches with suffix: " << suffix << std::endl;
    UnloadDevice();
    return 0;
}

static int SetActiveSlot(BShellHandle handle, int32_t argc, char *argv[])
{
    if (argc != PARTITION_ARGC) {
        BShellCmdHelp(handle, argc, argv);
        return -1;
    }
    if (LoadDevice() != 0) {
        std::cout << "Load partitionslot device failed" << std::endl;
        return -1;
    }
    std::cout << "Command: partitionslot setactive" << std::endl;
    int slot = atoi(argv[1]);
    sptr<IPartitionSlot> partitionslot = IPartitionSlot::Get();
    if (partitionslot == nullptr) {
        std::cout << "Get partitionslot failed" << std::endl;
        return -1;
    }
    partitionslot->SetActiveSlot(slot);
    std::cout << "Set active slot: " << slot << std::endl;
    UnloadDevice();
    return 0;
}

static int SetUnbootSlot(BShellHandle handle, int32_t argc, char *argv[])
{
    if (argc != PARTITION_ARGC) {
        BShellCmdHelp(handle, argc, argv);
        return -1;
    }
    if (LoadDevice() != 0) {
        std::cout << "Load partitionslot device failed" << std::endl;
        return -1;
    }
    std::cout << "Command: partitionslot setunboot" << std::endl;
    int slot = atoi(argv[1]);
    sptr<IPartitionSlot> partitionslot = IPartitionSlot::Get();
    if (partitionslot == nullptr) {
        std::cout << "Get partitionslot failed" << std::endl;
        return -1;
    }
    partitionslot->SetSlotUnbootable(slot);
    std::cout << "Set unboot slot: " << slot << std::endl;
    UnloadDevice();
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    CmdInfo infos[] = {
        {
            (char *)"partitionslot", GetSlot, (char *)"get the number of slots and current slot",
            (char *)"partitionslot getslot", (char *)"partitionslot getslot"
        },
        {
            (char *)"partitionslot", GetSuffix, (char *)"get suffix that matches with the slot",
            (char *)"partitionslot getsuffix [slot]", (char *)"partitionslot getsuffix"
        },
        {
            (char *)"partitionslot", SetActiveSlot, (char *)"set active slot",
            (char *)"partitionslot setactive [slot]", (char *)"partitionslot setactive"
        },
        {
            (char *)"partitionslot", SetUnbootSlot, (char *)"set unboot slot",
            (char *)"partitionslot setunboot [slot]", (char *)"partitionslot setunboot"
        }
    };
    for (size_t i = sizeof(infos) / sizeof(infos[0]); i > 0; i--) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i - 1]);
    }
}
