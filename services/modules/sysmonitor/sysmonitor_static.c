#include <string.h>
#include "init_module_engine.h"
#include "plugin_adapter.h"

static int SysmonitorEarlyHook(const HOOK_INFO *info, void *cookie)
{
    char enable[4] = {}; // 4 enable size
    uint32_t size = sizeof(enable);
    SystemReadParam("persist.init.sysmonitor.enabled", enable, &size);
    if (strcmp(enable, "1") != 0) {
        PLUGIN_LOGI("sysmonitor disabled.");
        return 0;
    }

    InitModuleMgrInstall("sysmonitor");
    PLUGIN_LOGI("sysmonitor enabled.");
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    // Depends on parameter service
    InitAddPostPersistParamLoadHook(0, SysmonitorEarlyHook);
}
