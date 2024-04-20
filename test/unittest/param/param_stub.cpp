/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "param_stub.h"
#include <dirent.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "begetctl.h"
#include "bootstage.h"
#include "init.h"
#include "init_log.h"
#include "init_param.h"
#ifndef OHOS_LITE
#include "init_mount.h"
#endif
#include "hookmgr.h"
#include "parameter.h"
#include "param_manager.h"
#include "param_security.h"
#include "param_utils.h"
#include "init_group_manager.h"
#include "init_module_engine.h"
#ifdef PARAM_LOAD_CFG_FROM_CODE
#include "param_cfg.h"
#endif
#include "ueventd.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

static int g_stubResult[STUB_MAX] = { 0 };
static int g_testRandom = 2; // 2 is test random

static int g_testPermissionResult = DAC_RESULT_PERMISSION;
void SetTestPermissionResult(int result)
{
    g_testPermissionResult = result;
}

static const char *selinuxLabels[][2] = {
    {"test.permission.read", "u:object_r:test_read:s0"},
    {"test.permission.write", "u:object_r:test_write:s0"},
    {"test.permission.watch", "u:object_r:test_watch:s0"},
};

static int TestGenHashCode(const char *buff)
{
    int code = 0;
    size_t buffLen = strlen(buff);
    for (size_t i = 0; i < buffLen; i++) {
        code += buff[i] - 'A';
    }
    return code;
}

static void TestSetSelinuxLogCallback(void) {}

static int TestSetParamCheck(const char *paraName, const char *context, const SrcInfo *info)
{
    BEGET_LOGI("TestSetParamCheck %s result %d", paraName, g_testPermissionResult);
    return g_testPermissionResult;
}

static const char *TestGetParamLabel(const char *paraName)
{
    BEGET_LOGI("TestGetParamLabel %s", paraName);
    if (paraName == nullptr) {
        return nullptr;
    }
    for (size_t i = 0; i < ARRAY_LENGTH(selinuxLabels); i++) {
        if (strncmp(selinuxLabels[i][0], paraName, strlen(selinuxLabels[i][0])) == 0) {
            return selinuxLabels[i][1];
        }
    }
    int code = TestGenHashCode(paraName);
    code = code % (ARRAY_LENGTH(selinuxLabels));
    return selinuxLabels[code][1];
}

static int32_t TestGetSelinuxLabelIndex(const char *paraName)
{
    for (size_t i = 0; i < ARRAY_LENGTH(selinuxLabels); i++) {
        if (strncmp(selinuxLabels[i][0], paraName, strlen(selinuxLabels[i][0])) == 0) {
            return i;
        }
    }
    int code = TestGenHashCode(paraName);
    code = code % (ARRAY_LENGTH(selinuxLabels));
    return code;
}

static const char *g_forbidReadParamName[] = {
    "ohos.servicectrl.",
    // "test.permission.write",
};
static int TestReadParamCheck(const char *paraName)
{
    // forbid to read ohos.servicectrl.
    for (size_t i = 0; i < ARRAY_LENGTH(g_forbidReadParamName); i++) {
        if (strncmp(paraName, g_forbidReadParamName[i], strlen(g_forbidReadParamName[i])) == 0) {
            return 1;
        }
    }
    return g_testPermissionResult;
}
static void TestDestroyParamList(ParamContextsList **list)
{
#ifdef PARAM_SUPPORT_SELINUX
    ParamContextsList *head = *list;
    while (head != nullptr) {
        ParamContextsList *next = head->next;
        free((void *)head->info.paraName);
        free((void *)head->info.paraContext);
        free(head);
        head = next;
    }
#endif
}
static ParamContextsList *TestGetParamList(void)
{
#ifdef PARAM_SUPPORT_SELINUX
    ParamContextsList *head = (ParamContextsList *)malloc(sizeof(ParamContextsList));
    BEGET_ERROR_CHECK(head != nullptr, return nullptr, "Failed to alloc ParamContextsList");
    head->info.paraName = strdup(selinuxLabels[0][0]);
    head->info.paraContext = strdup(selinuxLabels[0][1]);
    head->info.index = 0;
    head->next = nullptr;
    for (size_t i = 1; i < ARRAY_LENGTH(selinuxLabels); i++) {
        ParamContextsList *node = (ParamContextsList *)malloc(sizeof(ParamContextsList));
        BEGET_ERROR_CHECK(node != nullptr, TestDestroyParamList(&head);
            return nullptr, "Failed to alloc ParamContextsList");
        node->info.paraName = strdup(selinuxLabels[i][0]);
        node->info.paraContext = strdup(selinuxLabels[i][1]);
        node->info.index = i;
        node->next = head->next;
        head->next = node;
    }
    // test error, no node paraName
    ParamContextsList *node = (ParamContextsList *)malloc(sizeof(ParamContextsList));
    BEGET_ERROR_CHECK(node != nullptr, TestDestroyParamList(&head);
        return nullptr, "Failed to alloc ParamContextsList");
    node->info.paraName = nullptr;
    node->info.paraContext = strdup(selinuxLabels[0][1]);
    node->next = head->next;
    head->next = node;

    // test error, no node paraContext
    node = (ParamContextsList *)malloc(sizeof(ParamContextsList));
    BEGET_ERROR_CHECK(node != nullptr, TestDestroyParamList(&head);
        return nullptr, "Failed to alloc ParamContextsList");
    node->info.paraName = strdup(selinuxLabels[0][0]);
    node->info.paraContext = nullptr;
    node->next = head->next;
    head->next = node;

    // test error, repeat
    node = (ParamContextsList *)malloc(sizeof(ParamContextsList));
    BEGET_ERROR_CHECK(node != nullptr, TestDestroyParamList(&head);
        return nullptr, "Failed to alloc ParamContextsList");
    node->info.paraName = strdup(selinuxLabels[0][0]);
    node->info.paraContext = strdup(selinuxLabels[0][1]);
    node->info.index = 0;
    node->next = head->next;
    head->next = node;
    return head;
#else
    return nullptr;
#endif
}

void TestSetSelinuxOps(void)
{
#ifdef PARAM_SUPPORT_SELINUX
    SelinuxSpace *selinuxSpace = &GetParamWorkSpace()->selinuxSpace;
    selinuxSpace->setSelinuxLogCallback = TestSetSelinuxLogCallback;
    selinuxSpace->setParamCheck = TestSetParamCheck;
    selinuxSpace->getParamLabel = TestGetParamLabel;
    selinuxSpace->readParamCheck = TestReadParamCheck;
    selinuxSpace->getParamList = TestGetParamList;
    selinuxSpace->destroyParamList = TestDestroyParamList;
    selinuxSpace->getParamLabelIndex = TestGetSelinuxLabelIndex;
#endif
}

void TestSetParamCheckResult(const char *prefix, uint16_t mode, int result)
{
    ParamAuditData auditData = {};
    auditData.name = prefix;
    auditData.dacData.gid = 202;  // 202 test dac gid
    auditData.dacData.uid = 202;  // 202 test dac uid
    auditData.dacData.mode = mode;
    AddSecurityLabel(&auditData);
    SetTestPermissionResult(result);
}

void CreateTestFile(const char *fileName, const char *data)
{
    CheckAndCreateDir(fileName);
    PARAM_LOGV("PrepareParamTestData for %s", fileName);
    FILE *tmpFile = fopen(fileName, "wr");
    if (tmpFile != nullptr) {
        fprintf(tmpFile, "%s", data);
        (void)fflush(tmpFile);
        fclose(tmpFile);
    }
}
static void PrepareUeventdcfg(void)
{
    const char *ueventdcfg = "[device]\n"
        "/dev/test 0666 1000 1000\n"
        "[device]\n"
        "/dev/test1 0666 1000\n"
        "[device]\n"
        "/dev/test2 0666 1000 1000 1000 1000\n"
        "[sysfs]\n"
        "/dir/to/nothing attr_nowhere 0666 1000 1000\n"
        "[sysfs]\n"
        "  #/dir/to/nothing attr_nowhere 0666\n"
        "[sysfs\n"
        "/dir/to/nothing attr_nowhere 0666\n"
        "[firmware]\n"
        "/etc\n"
        "[device]\n"
        "/dev/testbinder 0666 1000 1000 const.dev.binder\n"
        "[device]\n"
        "/dev/testbinder1 0666 1000 1000 const.dev.binder\n"
        "[device]\n"
        "/dev/testbinder2 0666 1000 1000 const.dev.binder\n"
        "[device]\n"
        "/dev/testbinder3 0666 1000 1000 const.dev.binder\n";
    mkdir("/data/ueventd_ut", S_IRWXU | S_IRWXG | S_IRWXO);
    CreateTestFile(STARTUP_INIT_UT_PATH"/ueventd_ut/valid.config", ueventdcfg);
}
static void PrepareModCfg(void)
{
    const char *modCfg = "testinsmod";
    CreateTestFile(STARTUP_INIT_UT_PATH"/test_insmod", modCfg);
}
static void PrepareInnerKitsCfg()
{
    const char *innerKitsCfg = "/dev/block/platform/soc/10100000.himci.eMMC/by-name/system /system "
        "ext4 ro,barrier=1 wait\n"
        "/dev/block/platform/soc/10100000.himci.eMMC/by-name/vendor /vendor "
        "ext4 ro,barrier=1 wait\n"
        "/dev/block/platform/soc/10100000.himci.eMMC/by-name/hos "
        "/hos ntfs nosuid,nodev,noatime,barrier=1,data=ordered wait\n"
        "/dev/block/platform/soc/10100000.himci.eMMC/by-name/userdata /data ext4 "
        "nosuid,nodev,noatime,barrier=1,data=ordered,noauto_da_alloc "
        "wait,reservedsize=104857600\n"
        "  aaaa\n"
        "aa aa\n"
        "aa aa aa\n"
        "aa aa aa aa\n";
    const char *fstabRequired = "# fstab file.\n"
        "#<src> <mnt_point> <type> <mnt_flags and options> <fs_mgr_flags>\n"
        "/dev/block/platform/fe310000.sdhci/by-name/testsystem /usr ext4 ro,barrier=1 wait,required,nofail\n"
        "/dev/block/platform/fe310000.sdhci/by-name/testvendor /vendor ext4 ro,barrier=1 wait,required\n"
        "/dev/block/platform/fe310000.sdhci/by-name/testuserdata1 /data f2fs noatime,nosuid,nodev wait,check,quota\n"
        "/dev/block/platform/fe310000.sdhci/by-name/testuserdata2 /data ext4 noatime,fscrypt=xxx wait,check,quota\n"
        "/dev/block/platform/fe310000.sdhci/by-name/testmisc /misc none none wait,required";
    mkdir("/data/init_ut/mount_unitest/", S_IRWXU | S_IRWXG | S_IRWXO);
    CreateTestFile(STARTUP_INIT_UT_PATH"/mount_unitest/ReadFstabFromFile1.fstable", innerKitsCfg);
    CreateTestFile(STARTUP_INIT_UT_PATH"/etc/fstab.required", fstabRequired);
    CreateTestFile(STARTUP_INIT_UT_PATH"/system/etc/fstab.required", fstabRequired);
}
static void PrepareGroupTestCfg()
{
    const char *data = "{"
	    "\"jobs\": [\"param:job1\", \"param:job2\", \"param:job4\"],"
	    "\"services\": [\"service:service1\", \"service:service3\", \"service:service2\"],"
	    "\"groups\": [\"subsystem.xxx1.group\", \"subsystem.xxx2.group\", \"subsystem.xxx4.group\"]"
    "}";
    const char *xxx1 = "{"
	    "\"groups\": [\"subsystem.xxx11.group\""
    "}";
    const char *xxx11 = "{"
	    "\"groups\": [\"subsystem.xxx12.group\""
    "}";
    const char *xxx12 = "{"
	    "\"groups\": [\"subsystem.xxx13.group\""
    "}";
    const char *xxx13 = "{"
	    "\"groups\": [\"subsystem.xxx14.group\""
    "}";
    const char *xxx14 = "{"
	    "\"groups\": [\"subsystem.xxx11.group\""
    "}";
    CreateTestFile(GROUP_DEFAULT_PATH "/device.boot.group.cfg", data);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx1.group.cfg", xxx1);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx11.group.cfg", xxx11);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx12.group.cfg", xxx12);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx13.group.cfg", xxx13);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx14.group.cfg", xxx14);
}
static bool IsDir(const std::string &path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) < 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}
static bool DeleteDir(const std::string &path)
{
    auto pDir = std::unique_ptr<DIR, decltype(&closedir)>(opendir(path.c_str()), closedir);
    if (pDir == nullptr) {
        return false;
    }

    struct dirent *dp = nullptr;
    while ((dp = readdir(pDir.get())) != nullptr) {
        std::string currentName(dp->d_name);
        if (currentName[0] != '.') {
            std::string tmpName(path);
            tmpName.append("/" + currentName);
            if (IsDir(tmpName)) {
                DeleteDir(tmpName);
            }
            remove(tmpName.c_str());
        }
    }
    if (remove(path.c_str()) != 0) {
        return false;
    }
    return true;
}
static void LoadParamFromCfg(void)
{
#ifdef PARAM_LOAD_CFG_FROM_CODE
    for (size_t i = 0; i < ARRAY_LENGTH(g_paramDefCfgNodes); i++) {
        PARAM_LOGI("InitParamClient name %s = %s", g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
        uint32_t dataIndex = 0;
        int ret = WriteParam(g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value, &dataIndex, 0);
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d name %s %s",
            ret, g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
    }
#endif
}

static const char *g_triggerData = "{"
        "\"jobs\" : [{"
        "        \"name\" : \"early-init\","
        "        \"cmds\" : ["
        "            \"    write        '/proc/sys/kernel/sysrq 0'\","
        "            \"    load_persist_params \","
        "            \"    load_persist_params        \","
        "            \" #   load_persist_params \","
        "            \"   restorecon /postinstall\","
        "            \"mkdir /acct/uid\","
        "            \"chown root system /dev/memcg/memory.pressure_level\","
        "            \"chmod 0040 /dev/memcg/memory.pressure_level\","
        "            \"mkdir /dev/memcg/apps/ 0755 system system\","
        "           \"mkdir /dev/memcg/system 0550 system system\","
        "            \"start ueventd\","
        "            \"exec_start apexd-bootstrap\","
        "            \"setparam sys.usb.config ${persist.sys.usb.config}\""
        "        ]"
        "    },"
        "    {"
        "        \"name\" : \"param:trigger_test_1\","
        "        \"condition\" : \"test.sys.boot_from_charger_mode=5\","
        "        \"cmds\" : ["
        "            \"class_stop charger\","
        "            \"trigger late-init\""
        "        ]"
        "    },"
        "    {"
        "        \"name\" : \"param:trigger_test_2\","
        "        \"condition\" : \"test.sys.boot_from_charger_mode=1  "
        " || test.sys.boot_from_charger_mode=2   ||  test.sys.boot_from_charger_mode=3\","
        "        \"cmds\" : ["
        "            \"class_stop charger\","
        "            \"trigger late-init\""
        "        ]"
        "    },"
        "    {"
        "        \"name\" : \"load_persist_params_action\","
        "        \"cmds\" : ["
        "           \"load_persist_params\","
        "            \"start logd\","
        "            \"start logd-reinit\""
        "        ]"
        "    },"
        "    {"
        "        \"name\" : \"firmware_mounts_complete\","
        "        \"cmds\" : ["
        "            \"rm /dev/.booting\""
        "        ]"
        "    }"
        "]"
    "}";

void PrepareCmdLineData()
{
    const char *cmdLine = "bootgroup=device.boot.group earlycon=uart8250,mmio32,0xfe660000 "
        "root=PARTUUID=614e0000-0000 rw rootwait rootfstype=ext4 console=ttyFIQ0 hardware=rk3568 "
        "BOOT_IMAGE=/kernel init=/init default_boot_device=fe310000.sdhci bootslots=2 currentslot=1 initloglevel=2 "
        "ohos.required_mount.system="
        "/dev/block/platform/fe310000.sdhci/by-name/system@/usr@ext4@ro,barrier=1@wait,required "
        "ohos.required_mount.vendor="
        "/dev/block/platform/fe310000.sdhci/by-name/vendor@/vendor@ext4@ro,barrier=1@wait,required "
        "ohos.required_mount.misc="
        "/dev/block/platform/fe310000.sdhci/by-name/misc@none@none@none@wait,required ohos.boot.eng_mode=on ";
    CreateTestFile(BOOT_CMD_LINE, cmdLine);
}

static void PrepareAreaSizeFile(void)
{
    const char *ohosParamSize = "default_param=1024\n"
            "hilog_param=2048\n"
            "const_product_param=2048\n"
            "startup_param=20480\n"
            "persist_param=2048\n"
            "const_param=20480\n"
            "test_watch=81920\n"
            "test_write=81920\n"
            "const_param***=20480\n"
            "persist_sys_param=2048\n"
            "test_write=81920\n";
    CreateTestFile(PARAM_AREA_SIZE_CFG, ohosParamSize);
}

static void PrepareTestGroupFile(void)
{
    std::string groupData = "root:x:0:\n"
        "bin:x:2:\n"
        "system:x:1000:\n"
        "log:x:1007:\n"
        "deviceinfo:x:1102:\n"
        "samgr:x:5555:\n"
        "hdf_devmgr:x:3044:\n\n"
        "power_host:x:3025:\n"
        "servicectrl:x:1050:root,  shell,system,   samgr,   hdf_devmgr      \n"
        "powerctrl:x:1051:root, shell,system,  update,power_host\r\n"
        "bootctrl:x:1052:root,shell,system\n"
        "deviceprivate:x:1053:root,shell,system,samgr,hdf_devmgr,deviceinfo,"
        "dsoftbus,dms,account,useriam,access_token,device_manager,foundation,dbms,deviceauth,huks_server\n"
        "hiview:x:1201:\n"
        "hidumper_service:x:1212:\n"
        "shell:x:2000:\n"
        "cache:x:2001:\n"
        "net_bw_stats:x:3006:\n";

    CreateTestFile(STARTUP_INIT_UT_PATH "/etc/group", groupData.c_str());
}

static void PrepareDacData()
{
    // for dac
    std::string dacData = "ohos.servicectrl.   = system:servicectrl:0775 \n";
    dacData += "startup.service.ctl.        = system:servicectrl:0775:int\n";
    dacData += "test.permission.       = root:root:0770\n";
    dacData += "test.permission.read. =  root:root:0774\n";
    dacData += "test.permission.write.=  root:root:0772\n";
    dacData += "test.permission.watcher. = root:root:0771\n";
    dacData += "test.test1. = system:test1:0771\n";
    dacData += "test.test2.watcher. = test2:root:0771\n";
    dacData += "test.type.int. = root:root:0777:int\n";
    dacData += "test.type.bool. = root:root:0777:bool\n";
    dacData += "test.type.string. = root:root:0777\n";
    dacData += "test.invalid.int. = root:root:\n";
    dacData += "test.invalid.int. = root::\n";
    dacData += "test.invalid.int. = ::\n";
    dacData += "test.invalid.int. = \n";
    dacData += "test.invalid.int. \n";
    CreateTestFile(STARTUP_INIT_UT_PATH "/system/etc/param/ohos.para.dac", dacData.c_str());
    CreateTestFile(STARTUP_INIT_UT_PATH "/system/etc/param/ohos.para.dac_1", dacData.c_str());
}

static int TestHook(const HOOK_INFO *hookInfo, void *cookie)
{
    return 0;
}

void PrepareInitUnitTestEnv(void)
{
    static int evnOk = 0;
    if (evnOk) {
        return;
    }
    printf("PrepareInitUnitTestEnv \n");
#ifdef PARAM_SUPPORT_SELINUX
    RegisterSecuritySelinuxOps(nullptr, 0);
#endif

#ifndef OHOS_LITE
    InitAddGlobalInitHook(0, TestHook);
    InitAddPreParamServiceHook(0, TestHook);
    InitAddPreParamLoadHook(0, TestHook);
    InitAddPreCfgLoadHook(0, TestHook);
    InitAddPostCfgLoadHook(0, TestHook);
    InitAddPostPersistParamLoadHook(0, TestHook);
#endif
    // read default parameter from system
    LoadDefaultParams("/system/etc/param/ohos_const", LOAD_PARAM_NORMAL);
    LoadDefaultParams("/vendor/etc/param", LOAD_PARAM_NORMAL);
    LoadDefaultParams("/system/etc/param", LOAD_PARAM_ONLY_ADD);

    // read ut parameters
    LoadDefaultParams(STARTUP_INIT_UT_PATH "/system/etc/param/ohos_const", LOAD_PARAM_NORMAL);
    LoadDefaultParams(STARTUP_INIT_UT_PATH "/vendor/etc/param", LOAD_PARAM_NORMAL);
    LoadDefaultParams(STARTUP_INIT_UT_PATH "/system/etc/param", LOAD_PARAM_ONLY_ADD);
    LoadParamsFile(STARTUP_INIT_UT_PATH "/system/etc/param", LOAD_PARAM_ONLY_ADD);
    LoadParamFromCfg();

    int32_t loglevel = GetIntParameter("persist.init.debug.loglevel", INIT_ERROR);
    SetInitLogLevel((InitLogLevel)loglevel);

    // for test int get
    SystemWriteParam("test.int.get", "-101");
    SystemWriteParam("test.uint.get", "101");
    SystemWriteParam("test.string.get", "101");
    SystemWriteParam("test.bool.get.true", "true");
    SystemWriteParam("test.bool.get.false", "false");

    evnOk = 1;
}

int TestCheckParamPermission(const ParamLabelIndex *labelIndex,
    const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    // DAC_RESULT_FORBIDED
    return g_testPermissionResult;
}

int TestFreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    return 0;
}

void SetStubResult(STUB_TYPE type, int result)
{
    g_stubResult[type] = result;
}

#ifndef OHOS_LITE
static void TestBeforeInit(void)
{
    ParamWorkSpace *paramSpace = GetParamWorkSpace();
    EXPECT_NE(paramSpace, nullptr);
    InitParamService();
    CloseParamWorkSpace();
    paramSpace = GetParamWorkSpace();
    EXPECT_NE(paramSpace, nullptr);

    // test read cmdline
    Fstab *stab = LoadRequiredFstab();
    ReleaseFstab(stab);
}
#endif

static pid_t g_currPid = 0;
static __attribute__((constructor(101))) void ParamTestStubInit(void)
{
    g_currPid = getpid();
    printf("Init unit test start %u \n", g_currPid);
    EnableInitLog(INIT_INFO);
    InitParseGroupCfg();
    // prepare data
    mkdir(STARTUP_INIT_UT_PATH, S_IRWXU | S_IRWXG | S_IRWXO);
    CheckAndCreateDir(STARTUP_INIT_UT_PATH MODULE_LIB_NAME "/autorun/");
    int cmdIndex = 0;
    (void)GetMatchCmd("copy ", &cmdIndex);
    DoCmdByIndex(cmdIndex, MODULE_LIB_NAME"/libbootchart.z.so "
        STARTUP_INIT_UT_PATH MODULE_LIB_NAME "/libbootchart.z.so", nullptr);
    DoCmdByIndex(cmdIndex, MODULE_LIB_NAME"/libbootchart.z.so "
        STARTUP_INIT_UT_PATH MODULE_LIB_NAME "/autorun/libbootchart.z.so", nullptr);
    PrepareUeventdcfg();
    PrepareInnerKitsCfg();
    PrepareModCfg();
    PrepareGroupTestCfg();
    PrepareDacData();
    CreateTestFile(STARTUP_INIT_UT_PATH"/trigger_test.cfg", g_triggerData);
    PrepareAreaSizeFile();
    PrepareTestGroupFile();
    PrepareCmdLineData();
    TestSetSelinuxOps();

    PARAM_LOGI("TestSetSelinuxOps \n");
#ifndef OHOS_LITE
    TestBeforeInit();
#endif
#ifndef __LITEOS_A__
    SystemInit();
#endif
    PARAM_LOGI("SystemConfig \n");
    SystemConfig(NULL);
    PrepareInitUnitTestEnv();
}

__attribute__((destructor)) static void ParamTestStubExit(void)
{
    PARAM_LOGI("ParamTestStubExit %u %u \n", g_currPid, getpid());
    if (g_currPid != getpid()) {
        return;
    }
#ifndef OHOS_LITE
    StopParamService();

    HookMgrExecute(GetBootStageHookMgr(), INIT_BOOT_COMPLETE, nullptr, nullptr);
    CloseUeventConfig();
    CloseServiceSpace();
    demoExit();
    LE_CloseLoop(LE_GetDefaultLoop());
    HookMgrDestroy(GetBootStageHookMgr());
#endif
}

#ifdef OHOS_LITE
void __attribute__((weak))LE_DoAsyncEvent(const LoopHandle loopHandle, const TaskHandle taskHandle)
{
}

const char* HalGetSerial(void)
{
    static const char *serial = "1234567890";
    return serial;
}
#endif

int __attribute__((weak))SprintfStub(char *buffer, size_t size, const char *fmt, ...)
{
    int len = -1;
    va_list vargs;
    va_start(vargs, fmt);
    len = vsnprintf_s(buffer, size, size - 1, fmt, vargs);
    va_end(vargs);
    return len;
}

int __attribute__((weak))MountStub(const char* source, const char* target,
    const char* filesystemtype, unsigned long mountflags, const void * data)
{
    return g_stubResult[STUB_MOUNT];
}

int __attribute__((weak))UmountStub(const char *target)
{
    return 0;
}

int __attribute__((weak))Umount2Stub(const char *target, int flags)
{
    return 0;
}

int __attribute__((weak))SymlinkStub(const char * oldpath, const char * newpath)
{
    return 0;
}

int PrctlStub(int option, ...)
{
    if (option == PR_SET_SECUREBITS) {
        static int count = 0;
        count++;
        return (count % g_testRandom == 1) ? 0 : -1;
    }
    if (option == PR_CAP_AMBIENT) {
        static int count1 = 0;
        count1++;
        return (count1 % g_testRandom == 1) ? 0 : -1;
    }
    return 0;
}

int ExecvStub(const char *pathname, char *const argv[])
{
    printf("ExecvStub %s \n", pathname);
    return 0;
}

int LchownStub(const char *pathname, uid_t owner, gid_t group)
{
    return 0;
}

int KillStub(pid_t pid, int signal)
{
    return 0;
}

int ExecveStub(const char *pathname, char *const argv[], char *const envp[])
{
    printf("ExecveStub %s \n", pathname);
    return 0;
}

int LoadPolicy()
{
    return 0;
}

static int g_selinuxOptResult = 0;
int setcon(const char *name)
{
    g_selinuxOptResult++;
    return g_selinuxOptResult % g_testRandom;
}

int RestoreconRecurse(const char *name)
{
    g_selinuxOptResult++;
    return g_selinuxOptResult % g_testRandom;
}

int setexeccon(const char *name)
{
    g_selinuxOptResult++;
    return g_selinuxOptResult % g_testRandom;
}

int setsockcreatecon(const char *name)
{
    g_selinuxOptResult++;
    return g_selinuxOptResult % g_testRandom;
}

int setfilecon(const char *name, const char *content)
{
    g_selinuxOptResult++;
    return g_selinuxOptResult % g_testRandom;
}

ParamLabelIndex *TestGetParamLabelIndex(const char *name)
{
    static ParamLabelIndex labelIndex = {0};
    uint32_t index = 0;
    ParamWorkSpace *paramWorkspace = GetParamWorkSpace();
    if (paramWorkspace == nullptr) {
        return &labelIndex;
    }
#ifdef PARAM_SUPPORT_SELINUX
    if (paramWorkspace->selinuxSpace.getParamLabelIndex == nullptr) {
        return &labelIndex;
    }
    index = (uint32_t)paramWorkspace->selinuxSpace.getParamLabelIndex(name) + WORKSPACE_INDEX_BASE;
    if (index >= paramWorkspace->maxLabelIndex) {
        return &labelIndex;
    }
#endif
    labelIndex.workspace = paramWorkspace->workSpace[index];
    PARAM_CHECK(labelIndex.workspace != nullptr, return nullptr, "Invalid workSpace");
    labelIndex.selinuxLabelIndex = labelIndex.workspace->spaceIndex;
    (void)FindTrieNode(paramWorkspace->workSpace[0], name, strlen(name), &labelIndex.dacLabelIndex);
    return &labelIndex;
}
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
