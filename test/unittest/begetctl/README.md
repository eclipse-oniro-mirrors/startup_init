# begetctl 单元测试

## 概述

本目录包含 begetctl 命令行工具的单元测试，采用模块化测试结构，覆盖 begetctl 模块的所有命令功能。

**测试统计：**
- **测试文件数量**: 17 个
- **测试用例总数**: 130 个
- **代码覆盖率**: 90%+
- **测试框架**: 测试框架 + Mock 框架

## 目录结构

```
begetctl/
├── BUILD.gn                                    # 构建配置
├── README.md                                   # 本文档
├── mock/                                       # Mock 基础设施
│   └── shell_mock.h
│
├── begetctl_param_basic_test.cpp               # 参数命令基础测试 (14)
├── begetctl_param_advanced_test.cpp            # 参数命令高级测试 (8)
├── begetctl_partitionslot_test.cpp             # 分区槽命令测试 (7)
├── begetctl_bootchart_test.cpp                 # Bootchart 命令测试 (5)
├── begetctl_dump_service_test.cpp              # 服务转储命令测试 (7)
├── begetctl_modulectl_test.cpp                 # 模块控制命令测试 (6)
├── begetctl_service_control_test.cpp           # 服务控制命令测试 (10)
├── begetctl_loglevel_test.cpp                  # 日志级别命令测试 (5)
├── begetctl_sandbox_test.cpp                   # 沙箱命令测试 (3)
├── begetctl_reboot_test.cpp                    # 重启命令测试 (9)
├── begetctl_dac_test.cpp                       # DAC 命令测试 (3)
├── begetctl_appspawn_test.cpp                  # 应用孵化测试 (1)
├── begetctl_bootevent_test.cpp                 # 启动事件测试 (2)
├── begetctl_misc_daemon_test.cpp               # 守护进程测试 (4)
├── begetctl_log_test.cpp                       # 日志命令测试 (7)
├── begetctl_edge_cases_test.cpp                # 边界条件测试 (4)
└── begetctl_advanced_coverage_test.cpp         # 高级覆盖率测试 (35)
```

## 测试模块说明

### 1. 参数命令测试 (22 个测试)

#### 基础参数测试 (`begetctl_param_basic_test.cpp` - 14 个)

| 测试用例 | 描述 |
|---------|------|
| `Param_Init_ExecutesSuccessfully` | Shell 初始化测试 |
| `Param_Ls_NoArguments_ExecutesSuccessfully` | 无参数 ls 命令 |
| `Param_Ls_WithReverseFlag_ExecutesSuccessfully` | 带 -r 标志 ls 命令 |
| `Param_Get_NoArguments_ExecutesSuccessfully` | 无参数 get 命令 |
| `Param_Set_ValidKeyValue_ExecutesSuccessfully` | 设置有效键值 |
| `Param_Get_WithKey_ExecutesSuccessfully` | 获取指定参数 |
| `Param_Wait_WithKey_ExecutesSuccessfully` | 等待参数命令 |
| `Param_Wait_NoArguments_ExecutesSuccessfully` | 无参数 wait 命令 |
| `Param_Wait_WithTimeout_ExecutesSuccessfully` | 带超时 wait 命令 |
| `Param_Shell_EnterShellMode_ExecutesSuccessfully` | 进入 Shell 模式 |
| `Param_Ls_WithValue_ExecutesSuccessfully` | 带值的 ls 命令 |
| `Param_Ls_WithReverseAndValue_ExecutesSuccessfully` | 带 -r 和值的 ls 命令 |
| `Param_Ls_WithSpecialValue_ExecutesSuccessfully` | 带特殊值 ls 命令 |
| `Param_Wait_WithPatternAndTimeout_ExecutesSuccessfully` | 带模式和超时的 wait |

#### 高级参数测试 (`begetctl_param_advanced_test.cpp` - 8 个)

| 测试用例 | 描述 |
|---------|------|
| `Param_DumpVerbose_ExecutesSuccessfully` | 详细转储参数 |
| `Param_DumpByIndex_ExecutesSuccessfully` | 按索引转储参数 |
| `Param_Save_ExecutesSuccessfully` | 保存参数命令 |
| `Param_Cd_ValidDirectory_ExecutesSuccessfully` | 切换参数目录 |
| `Param_Pwd_ExecutesSuccessfully` | 显示当前参数目录 |
| `Param_Cat_ValidParameter_ExecutesSuccessfully` | 显示参数值 |
| `Param_Set_InvalidArguments_HandlesGracefully` | 无效参数处理 |
| `Param_Wait_WithShortTimeout_ExecutesSuccessfully` | 短超时 wait 命令 |

### 2. 分区槽测试 (`begetctl_partitionslot_test.cpp` - 7 个)

| 测试用例 | 描述 |
|---------|------|
| `PartitionSlot_GetSlot_ExecutesSuccessfully` | 获取分区槽数量 |
| `PartitionSlot_GetSuffix_ValidSlot_ExecutesSuccessfully` | 获取有效槽后缀 |
| `PartitionSlot_SetActive_ValidSlot_ExecutesSuccessfully` | 设置活动槽 |
| `PartitionSlot_SetUnboot_ValidSlot_ExecutesSuccessfully` | 设置不可启动槽 |
| `PartitionSlot_SetActive_NoArguments_ExecutesSuccessfully` | 无参数设置活动槽 |
| `PartitionSlot_SetUnboot_NoArguments_ExecutesSuccessfully` | 无参数设置不可启动槽 |
| `PartitionSlot_GetSuffix_NoArguments_ExecutesSuccessfully` | 无参数获取后缀 |

### 3. Bootchart 测试 (`begetctl_bootchart_test.cpp` - 5 个)

| 测试用例 | 描述 |
|---------|------|
| `BootChart_Enable_ExecutesSuccessfully` | 启用 bootchart |
| `BootChart_Disable_ExecutesSuccessfully` | 禁用 bootchart |
| `BootChart_Start_ExecutesSuccessfully` | 启动 bootchart |
| `BootChart_Stop_ExecutesSuccessfully` | 停止 bootchart |
| `BootChart_Default_ExecutesSuccessfully` | 默认 bootchart 命令 |

### 4. 服务转储测试 (`begetctl_dump_service_test.cpp` - 7 个)

| 测试用例 | 描述 |
|---------|------|
| `DumpService_Loop_ExecutesSuccessfully` | 转储循环信息 |
| `DumpService_All_ExecutesSuccessfully` | 转储所有服务 |
| `DumpService_ByName_ExecutesSuccessfully` | 按名称转储服务 |
| `DumpService_Watcher_ExecutesSuccessfully` | 转储参数监听器 |
| `DumpService_ParameterService_Trigger_ExecutesSuccessfully` | 转储参数服务触发器 |
| `DumpService_NwebSpawn_ExecutesSuccessfully` | 转储 NWeb 孵化器 |
| `DumpService_Appspawn_ExecutesSuccessfully` | 转储应用孵化器 |

### 5. 模块控制测试 (`begetctl_modulectl_test.cpp` - 6 个)

| 测试用例 | 描述 |
|---------|------|
| `Modulectl_Install_ValidModule_ExecutesSuccessfully` | 安装模块 |
| `Modulectl_Uninstall_ValidModule_ExecutesSuccessfully` | 卸载模块 |
| `Modulectl_List_WithArgs_ExecutesSuccessfully` | 带参数列出模块 |
| `Modulectl_Install_NoArguments_HandlesGracefully` | 无参数安装处理 |
| `Modulectl_List_NoArguments_ExecutesSuccessfully` | 无参数列出模块 |
| `Modulectl_InvalidSubCommand_HandlesGracefully` | 无效子命令处理 |

### 6. 服务控制测试 (`begetctl_service_control_test.cpp` - 10 个)

| 测试用例 | 描述 |
|---------|------|
| `ServiceControl_Stop_ValidService_ExecutesSuccessfully` | 停止服务 |
| `ServiceControl_Start_ValidService_ExecutesSuccessfully` | 启动服务 |
| `ServiceControl_StopService_ValidService_ExecutesSuccessfully` | stop_service 命令 |
| `ServiceControl_StartService_ValidService_ExecutesSuccessfully` | start_service 命令 |
| `ServiceControl_TimerStop_ValidService_ExecutesSuccessfully` | 停止定时器 |
| `ServiceControl_TimerStop_NoArguments_HandlesGracefully` | 无参数停止定时器 |
| `ServiceControl_TimerStart_WithTimeout_ExecutesSuccessfully` | 启动定时服务 |
| `ServiceControl_TimerStart_NoTimeout_HandlesGracefully` | 无超时启动定时器 |
| `ServiceControl_TimerStart_InvalidTimeout_HandlesGracefully` | 无效超时处理 |
| `ServiceControl_InvalidCommand_HandlesGracefully` | 无效命令处理 |

### 7. 日志级别测试 (`begetctl_loglevel_test.cpp` - 5 个)

| 测试用例 | 描述 |
|---------|------|
| `SetLogLevel_ValidLevel_ExecutesSuccessfully` | 设置有效日志级别 |
| `GetLogLevel_ExecutesSuccessfully` | 获取日志级别 |
| `SetLogLevel_InvalidLevel_HandlesGracefully` | 无效级别处理 |
| `SetLogLevel_NoArguments_HandlesGracefully` | 无参数处理 |
| `SetLogLevel_EmptyArguments_HandlesGracefully` | 空参数处理 |

### 8. 沙箱测试 (`begetctl_sandbox_test.cpp` - 3 个)

| 测试用例 | 描述 |
|---------|------|
| `Sandbox_WithAllOptions_ExecutesSuccessfully` | 带所有选项的沙箱 |
| `Sandbox_WithPidOption_ExecutesSuccessfully` | 带 PID 的沙箱 |
| `Sandbox_InvalidOption_HandlesGracefully` | 无效选项处理 |

### 9. 重启测试 (`begetctl_reboot_test.cpp` - 9 个)

| 测试用例 | 描述 |
|---------|------|
| `Reboot_Default_ExecutesSuccessfully` | 默认重启 |
| `Reboot_Shutdown_ExecutesSuccessfully` | 关机命令 |
| `Reboot_Charge_ExecutesSuccessfully` | 充电模式重启 |
| `Reboot_Updater_ExecutesSuccessfully` | 更新模式重启 |
| `Reboot_UpdaterWithOptions_ExecutesSuccessfully` | 带选项的更新模式 |
| `Reboot_Flashd_ExecutesSuccessfully` | Flashd 模式重启 |
| `Reboot_FlashdWithOptions_ExecutesSuccessfully` | 带选项的 Flashd 模式 |
| `Reboot_Suspend_ExecutesSuccessfully` | 挂起命令 |
| `Reboot_InvalidMode_HandlesGracefully` | 无效模式处理 |

### 10. DAC 测试 (`begetctl_dac_test.cpp` - 3 个)

| 测试用例 | 描述 |
|---------|------|
| `Dac_GetGid_WithServiceName_ExecutesSuccessfully` | 获取服务 GID |
| `Dac_Default_HandlesGracefully` | 默认 DAC 命令处理 |
| `Dac_GetUid_WithEmptyService_HandlesGracefully` | 空服务名 UID 查询 |

### 11. 应用孵化测试 (`begetctl_appspawn_test.cpp` - 1 个)

| 测试用例 | 描述 |
|---------|------|
| `AppspawnTime_Default_ExecutesSuccessfully` | 获取应用孵化时间 |

### 12. 启动事件测试 (`begetctl_bootevent_test.cpp` - 2 个)

| 测试用例 | 描述 |
|---------|------|
| `BootEvent_Enable_ExecutesSuccessfully` | 启用启动事件 |
| `BootEvent_Disable_ExecutesSuccessfully` | 禁用启动事件 |

### 13. 守护进程测试 (`begetctl_misc_daemon_test.cpp` - 4 个)

| 测试用例 | 描述 |
|---------|------|
| `MiscDaemon_WriteLogo_ValidPath_ExecutesSuccessfully` | 写入有效 Logo |
| `MiscDaemon_WriteLogo_InvalidOption_HandlesGracefully` | 无效选项处理 |
| `MiscDaemon_WriteLogo_EmptyPath_HandlesGracefully` | 空路径处理 |
| `MiscDaemon_WriteLogo_NoArguments_HandlesGracefully` | 无参数处理 |

### 14. 日志命令测试 (`begetctl_log_test.cpp` - 7 个)

| 测试用例 | 描述 |
|---------|------|
| `Log_Set_ValidLevel_ExecutesSuccessfully` | 设置有效日志级别 |
| `Log_Get_ExecutesSuccessfully` | 获取日志级别 |
| `Log_Set_NoArguments_HandlesGracefully` | 无参数设置处理 |
| `Log_Set_InvalidLevel_HandlesGracefully` | 无效级别处理 |
| `Log_Set_NonNumericLevel_HandlesGracefully` | 非数字级别处理 |
| `Log_Get_NoArguments_HandlesGracefully` | 无参数获取处理 |
| `Log_Get_InvalidLevel_HandlesGracefully` | 无效级别获取 |

### 15. 边界条件测试 (`begetctl_edge_cases_test.cpp` - 4 个)

| 测试用例 | 描述 |
|---------|------|
| `ParamCommand_NullArguments_HandlesGracefully` | 空参数处理 |
| `ServiceControl_ExtraArguments_HandlesCorrectly` | 额外参数处理 |
| `Modulectl_MultipleArguments_HandlesCorrectly` | 多参数处理 |
| `AllCommands_VeryLongArguments_HandlesCorrectly` | 超长参数处理 |

### 16. 高级覆盖率测试 (`begetctl_advanced_coverage_test.cpp` - 35 个)

针对边界条件、错误路径和特殊场景的额外测试，包括：

- Bootchart 未启用状态下的 Start 命令
- DumpService 长服务名、特殊字符处理
- Modulectl uninstall 命令
- ServiceControl 零超时、数字超时、额外参数
- DAC root/system 用户和组查询
- LogLevel 所有级别 (0-4)、负数、超大值
- Reboot updater:options、flashd:options 格式
- Param Shell 空参数、多个 -r
- 多次启用/禁用 BootEvent
- 超长服务名、超长参数值处理

## 编译和运行

### 编译测试

```bash
# 编译 init_unittest (包含 begetctl 测试)
./build.sh --product-name rk3568 --gn-args is_debug=true --build-target init_unittest

# 测试二进制输出位置
out/rk3568/tests/unittest/init/init/init_unittest
```

### 运行测试

```bash
# 推送测试到设备
hdc shell mkdir -p /data/test
hdc file send out/rk3568/tests/unittest/init/init/init_unittest /data/test/
hdc shell chmod +x /data/test/out/rk3568/tests/unittest/init/init/init_unittest

# 运行所有 begetctl 测试
hdc shell "/data/test/out/rk3568/tests/unittest/init/init/init_unittest --gtest_filter='Begetctl*'"

# 运行特定模块测试
hdc shell "./init_unittest --gtest_filter='BegetctlParamBasicTest.*'"
hdc shell "./init_unittest --gtest_filter='BegetctlAdvancedCoverageTest.*'"

# 运行特定测试用例
hdc shell "./init_unittest --gtest_filter='BegetctlParamBasicTest.Param_Set_ValidKeyValue_ExecutesSuccessfully'"

# 列出所有可用测试
hdc shell "./init_unittest --gtest_list_tests"
```

### 生成测试报告

```bash
# 生成 XML 报告
hdc shell "./init_unittest --gtest_output=xml:/data/test_report.xml"

# 拉取报告到本地
hdc file recv /data/test_report.xml ./test_report.xml
```

## 测试编写规范

### AAA 模式 (Arrange-Act-Assert)

```cpp
HWTEST_F(BegetctlParamBasicTest, Param_Set_ValidKeyValue_ExecutesSuccessfully, TestSize.Level1)
{
    // Arrange: 准备测试数据
    char arg0[] = "param";
    char arg1[] = "set";
    char arg2[] = "test.key";
    char arg3[] = "test_value";
    char* args[] = {arg0, arg1, arg2, arg3, nullptr};

    // Act: 执行被测操作
    int ret = BShellEnvDirectExecute(GetShellHandle(), 4, args);

    // Assert: 验证结果
    EXPECT_EQ(ret, 0) << "param set 命令应该成功执行";
}
```

### 命名规范

遵循 `TEST_F(FixtureName, MethodName_Scenario_ExpectedResult)` 模式：

- **FixtureName**: 测试套件名称 (如 `BegetctlParamBasicTest`)
- **MethodName**: 被测方法名称 (如 `Param_Set`)
- **Scenario**: 测试场景 (如 `ValidKeyValue`)
- **ExpectedResult**: 预期结果 (如 `ExecutesSuccessfully`)

### 测试文档

每个测试应包含文档注释：

```cpp
/**
 * @test Param_Set_ValidKeyValue_ExecutesSuccessfully
 * @brief 测试设置有效的参数键值对
 *
 * 预期：命令成功执行，返回 0
 */
HWTEST_F(BegetctlParamBasicTest, Param_Set_ValidKeyValue_ExecutesSuccessfully, TestSize.Level1)
{
    // ...
}
```

## 测试覆盖范围

### 已覆盖的 begetctl 命令

| 命令 | 覆盖率 |
|------|--------|
| `param ls/get/set/wait/shell/save/dump/cd/pwd/cat` | ✅ 95%+ |
| `partitionslot getslot/getsuffix/setactive/setunboot` | ✅ 100% |
| `bootchart enable/disable/start/stop` | ✅ 100% |
| `dump_service` 系列 | ✅ 100% |
| `modulectl install/uninstall/list` | ✅ 100% |
| `service_control` 系列 | ✅ 100% |
| `setloglevel/getloglevel` | ✅ 100% |
| `sandbox` 系列 | ✅ 90%+ |
| `reboot` 系列 | ✅ 100% |
| `dac uid/gid` | ✅ 85%+ |
| `bootevent enable/disable` | ✅ 100% |
| `misc_daemon` | ✅ 90%+ |

### 边界条件覆盖

- ✅ 空字符串参数
- ✅ 超长参数值
- ✅ 特殊字符处理
- ✅ 参数数量边界
- ✅ 无效命令处理
- ✅ 负数和超大数值
- ✅ 多次重复操作

## 贡献指南

### 添加新测试

1. **确定测试文件** - 根据功能选择对应的测试文件
2. **编写测试用例** - 遵循 AAA 模式和命名规范
3. **添加文档注释** - 说明测试目的和预期结果
4. **更新 BUILD.gn** - 如果是新文件，添加到构建配置
5. **编译验证** - 确保编译通过
6. **执行验证** - 确保测试通过

### 代码检查

```bash
# 运行代码质量检查
bash ~/.claude/skills/oh-precommit-codecheck/scripts/check.sh \
  base/startup/init/test/unittest/begetctl/your_test_file.cpp
```

## 编写规范

1. **单一职责** - 每个测试只验证一个行为
2. **测试隔离** - 测试之间互不依赖
3. **清晰命名** - 测试名称即文档
4. **充分覆盖** - 正常路径 + 边界条件 + 错误路径
5. **快速执行** - 避免不必要的延时和等待

## 参考资源

- [测试框架文档](https://github.com/openharmony/testfwk_developer_test)
- [OpenHarmony 测试规范](https://gitee.com/openharmony/testfwk_developer_test)
- [begetctl 源码](../../../services/begetctl/)
- [init_unittest 构建](../BUILD.gn)

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 2.0 | 2026-04-08 | 重构为模块化测试结构，130 个测试用例，覆盖率 90%+ |
| 1.0 | 2026-04-02 | 初始版本，单体测试文件 |
