/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/statvfs.h>
#include <string>
#include "init_cmds.h"
#include "init_param.h"
#include "init_group_manager.h"
#include "init_utils.h"
#include "dowrite_fuzzer.h"


namespace OHOS {
    static int DoCmdByName(const char *name, const char *cmdContent)
    {
        int cmdIndex = 0;
        (void)GetMatchCmd(name, &cmdIndex);
        DoCmdByIndex(cmdIndex, cmdContent, nullptr);
        return 0;
    }

    bool FuzzDoWrite(const uint8_t* data, size_t size)
    {
        if (size <= 0) {
            return false;
        }
        std::string str(reinterpret_cast<const char*>(data), size);
        int ret = DoCmdByName("write ", "/data/init_ut/test_dir0/test_file_write1 aaa");
        int ret2 = DoCmdByName("write ", str.c_str());
        return (ret == 0 && ret2 == 0);
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzDoWrite(data, size);
    return 0;
}
