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
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>
#include "init_unittest.h"
#include "cJSON.h"
#include "sandbox.h"
#include "sandbox_namespace.h"
#include "securec.h"

// constexpr static mode_t FILE_MODE = 0766;
using namespace testing::ext;
namespace init_ut {
const int NULL_ROOT_PATH = 1;
const int NULL_MOUNT_FLAGS = 2;
const int NULL_MOUNT = 3;
const int NULL_MOUNT_ITEM = 13;
const int LINK_ARRARY_START = 5;
const int LINK_ARRARY_END = 8;
const int FLAGS_NUMBER = 3;

const char *test_sandbox_name = "test";
const char *sandbox_json_path = "test-sandbox.json";


void RestartSandbox(const char *sandbox)
{
    if (sandbox == nullptr) {
        std::cout << "invalid parameters" << std::endl;
        return;
    }
    InitDefaultNamespace();
    std::cout << "init namespace" << std::endl;
    if (!InitSandboxWithName(sandbox)) {
        CloseDefaultNamespace();
        std::cout << "Failed to init sandbox with name " << sandbox << std::endl;
        return;
    }
    std::cout << "init sandbox with name" << std::endl;
    DumpSandboxByName(sandbox);
    std::cout << "dump sandbox" << std::endl;
    if (PrepareSandbox(sandbox) != 0) {
        std::cout << "Failed to prepare sandbox %s" << sandbox << std::endl;
        DestroySandbox(sandbox);
        CloseDefaultNamespace();
        return;
    }
    std::cout << "prepare sandbox" << std::endl;
    if (EnterDefaultNamespace() < 0) {
        std::cout << "Failed to set default namespace" << std::endl;
        DestroySandbox(sandbox);
        CloseDefaultNamespace();
        return;
    }
    std::cout << "enter default namespace" << std::endl;
    CloseDefaultNamespace();
    std::cout << "close namespace" << std::endl;
}

cJSON *MakeSandboxJson(const char *SandboxFileName, const int MODE)
{
    const char *SANDBOX_CONFIG[] = {"sandbox-root", "mount-bind-paths", "mount-bind-files", "symbol-links"};
    const char *SANDBOX_ROOT[] = { "/mnt/sandbox/test", "/mnt/sandbox/chipset", "/mnt/error"};
    const char *SANDBOX_FLAGS[] = {"bind", "rec", "private"};
    const char *MOUNT_BIND_PATHS[] = {"src-path", "sandbox-path", "sandbox-flags"};
    const char *SYMBOL_LINKS[] = {"target-name", "link-name"};
    const char *APP_PATHS[] = {"/mnt", "/sys", "/proc", "/dev", "/data",
                               "/system/bin", "/system/lib", "/system/etc", "/system"};

    cJSON *mJsonSandbox = cJSON_CreateObject();             // json file object
    cJSON *mJsonMtBdPth = cJSON_CreateArray();              // mount-bind-paths
    cJSON *mJsonMtBdFl = cJSON_CreateArray();               // mount-bind-files
    cJSON *mJsonSymLk = cJSON_CreateArray();                // symbol-links
    cJSON *mJsonMtBdPth_Itm_SdxFlg = cJSON_CreateArray();   // mount-bind-paths items sandbox-flags
    cJSON *mJsonMtBdFl_Itm = cJSON_CreateObject();          // mount-bind-files items
    cJSON *mJsonMtBdPth_Itm;                                // point to mount-bind-paths items
    cJSON *mJsonSymLk_Itm;                                  // point to symbol-links items

    //  drop root path
    if (MODE != NULL_ROOT_PATH) {
        cJSON_AddItemToObject(mJsonSandbox, SANDBOX_CONFIG[0], cJSON_CreateString(SANDBOX_ROOT[0]));
    }

    // assemble SANDBOX_FLAGS
    if (MODE != NULL_MOUNT_FLAGS) {
        for (int i = 0; i < FLAGS_NUMBER; i++) {
            cJSON_AddItemToArray(mJsonMtBdPth_Itm_SdxFlg, cJSON_CreateString(SANDBOX_FLAGS[i]));
        }
    }

    // assemble mount-bind-paths items
    // Append items to mount-bind-paths
    for (int i = 0; i < (sizeof(APP_PATHS) / sizeof(char *)); i++) {
        cJSON_AddItemToArray(mJsonMtBdPth, mJsonMtBdPth_Itm = cJSON_CreateObject());
        int MOUNT_FLAG_COUNT = 2;
        if (MODE != NULL_MOUNT_ITEM) {
            cJSON_AddItemToObject(mJsonMtBdPth_Itm, MOUNT_BIND_PATHS[0], cJSON_CreateString(APP_PATHS[i]));
            cJSON_AddItemToObject(mJsonMtBdPth_Itm, MOUNT_BIND_PATHS[1], cJSON_CreateString(APP_PATHS[i]));
        } else {
            cJSON_AddItemToObject(mJsonMtBdPth_Itm, MOUNT_BIND_PATHS[0], nullptr);
            cJSON_AddItemToObject(mJsonMtBdPth_Itm, MOUNT_BIND_PATHS[1], nullptr);
        }
        cJSON_AddItemToObject(mJsonMtBdPth_Itm, MOUNT_BIND_PATHS[MOUNT_FLAG_COUNT], mJsonMtBdPth_Itm_SdxFlg);
    }

    if (MODE != NULL_MOUNT) {
        // Append items to mount-bind-files
        cJSON_AddItemToArray(mJsonMtBdFl, mJsonMtBdFl_Itm);
        // assemble symbol-links items
        for (int i = LINK_ARRARY_START; i < LINK_ARRARY_END; i++) {
            // Append items to symbol-links
            cJSON_AddItemToArray(mJsonSymLk, mJsonSymLk_Itm = cJSON_CreateObject());
            cJSON_AddItemToObject(mJsonSymLk_Itm, SYMBOL_LINKS[0], cJSON_CreateString(APP_PATHS[i]));
            cJSON_AddItemToObject(mJsonSymLk_Itm, SYMBOL_LINKS[1], cJSON_CreateString(APP_PATHS[i]));
        }
    }

    // at last, assemble the json file
    int count = 1;
    cJSON_AddItemToObject(mJsonSandbox, SANDBOX_CONFIG[count++], mJsonMtBdPth);
    cJSON_AddItemToObject(mJsonSandbox, SANDBOX_CONFIG[count++], mJsonMtBdFl);
    cJSON_AddItemToObject(mJsonSandbox, SANDBOX_CONFIG[count], mJsonSymLk);
    return mJsonSandbox;
}

bool makeFileByJson(cJSON * mJson, const char *SandboxFileName)
{
    std::string const SANDBOX_JSON_PATH = std::string("/etc/sandbox/") + std::string(SandboxFileName);
    const char* c_SANDBOX_JSON_PATH = SANDBOX_JSON_PATH.c_str();

    int fd = open(c_SANDBOX_JSON_PATH, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0766);
    if (fd < 0) {
        std::cout << "open sandbox json file failed" << std::endl;
        return false;
    }

    char *cjValue1 = cJSON_Print(mJson);
    int ret1 = write(fd, cjValue1, strlen(cjValue1));
    if (-1 == ret1) {
        std::cout << "Write file ERROR" << errno << "  fd is :" << fd << std::endl;
        return false;
    }
    free(cjValue1);
    return true;
}

class SandboxUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(SandboxUnitTest, TestCreateNormalSandbox, TestSize.Level1) {
    cJSON *mJson = MakeSandboxJson(sandbox_json_path, 0);
    makeFileByJson(mJson, sandbox_json_path);
    RestartSandbox(test_sandbox_name);
}

HWTEST_F(SandboxUnitTest, TestEnterErrorSandbox, TestSize.Level1) {
    int ret1 = EnterSandbox("error_system");
    ASSERT_EQ(ret1, -1);
    const char *pname = nullptr;
    int ret2 = EnterSandbox(pname);
    ASSERT_EQ(ret2, -1);
    DestroySandbox(test_sandbox_name);
    int ret3 = EnterSandbox(test_sandbox_name);
    ASSERT_EQ(ret3, -1);
}

HWTEST_F(SandboxUnitTest, TestCreateErrorSandbox1, TestSize.Level1) {
    const char *pname = nullptr;
    std::cout << "test destory nullptr" << std::endl;
    DestroySandbox(pname);
    std::cout << "test destory xapp" << std::endl;
    DestroySandbox("xapp");
    std::cout << "test enter xapp" << std::endl;
    int ret1 = EnterSandbox("xapp");
    ASSERT_EQ(ret1, -1);
    bool result = InitSandboxWithName(pname);
    ASSERT_FALSE(result);
    DumpSandboxByName(pname);
    DumpSandboxByName("xpp");
    result  = InitSandboxWithName("xapp");
    ASSERT_FALSE(result);
}

HWTEST_F(SandboxUnitTest, TestCreateErrorSandbox2, TestSize.Level1) {
    cJSON *mJson = MakeSandboxJson(sandbox_json_path, NULL_ROOT_PATH);
    bool ret1 = makeFileByJson(mJson, sandbox_json_path);
    ASSERT_TRUE(ret1);
    InitSandboxWithName(test_sandbox_name);
    int ret = PrepareSandbox(test_sandbox_name);
    ASSERT_EQ(ret, -1);
    ret = PrepareSandbox("xapp");
    ASSERT_EQ(ret, -1);
}

HWTEST_F(SandboxUnitTest, TestCreateSandboxNoneJsonError, TestSize.Level1) {
    unlink("/etc/sandbox/test-sandbox.json");
    int ret = PrepareSandbox(test_sandbox_name);
    ASSERT_EQ(ret, -1);
}

HWTEST_F(SandboxUnitTest, TestCreateSandboxMountFlagsError, TestSize.Level1) {
    cJSON *mJson = MakeSandboxJson(sandbox_json_path, NULL_MOUNT_FLAGS);
    makeFileByJson(mJson, sandbox_json_path);
    int ret = PrepareSandbox(test_sandbox_name);
    ASSERT_EQ(ret, -1);
}

HWTEST_F(SandboxUnitTest, TestCreateSandboxMountNULLError, TestSize.Level1) {
    cJSON *mJson = MakeSandboxJson(sandbox_json_path, NULL_MOUNT_ITEM);
    makeFileByJson(mJson, sandbox_json_path);
    int ret = PrepareSandbox(test_sandbox_name);
    ASSERT_EQ(ret, -1);
    InitSandboxWithName(test_sandbox_name);
    ASSERT_EQ(ret, -1);
}

HWTEST_F(SandboxUnitTest, TestUnshareNamespace, TestSize.Level1) {
    int ret1 = UnshareNamespace(-1);
    ASSERT_EQ(ret1, -1);
}

HWTEST_F(SandboxUnitTest, TestSetNamespace, TestSize.Level1) {
    int ret1 = SetNamespace(-1, 1);
    ASSERT_EQ(ret1, -1);
    ret1 = SetNamespace(1, -1);
    ASSERT_EQ(ret1, -1);
}

HWTEST_F(SandboxUnitTest, TestGetNamespaceFd, TestSize.Level1) {
    int ret1 = GetNamespaceFd("");
    ASSERT_EQ(ret1, -1);
}
}
