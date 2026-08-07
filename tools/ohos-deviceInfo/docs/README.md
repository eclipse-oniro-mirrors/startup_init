# ohos-deviceInfo

## 概述

`ohos-deviceInfo` 是一个 OpenHarmony 设备信息查询 CLI 工具，封装 `libbegetutil` 的 11 个设备信息 getter 接口，提供 OS 版本和发行版信息的只读查询能力。所有接口均为纯同步调用，直接读取系统参数共享内存，无 IPC、无事件循环、无权限要求。

## 功能列表

- 查询单个设备信息字段（`get` 命令，11 个字段可选）
- 批量查询全部设备信息字段（`all` 命令，一次性返回 11 个字段）
- 提供命令行帮助（`--help`）

## 依赖

- 系统能力：`libbegetutil`（`syspara/parameter.h`）
- 权限：无（CLI 不涉及任何 `ohos.permission` 权限）
- 构建依赖：`cJSON:cjson`、`bounds_checking_function:libsec_shared`

## 基本用法

```bash
ohos-deviceInfo <command> [options]
```

## 字段列表（11 个）

### 字符串型字段（7 个）

| 字段名 | 对应接口 |
|--------|----------|
| osFullName | GetOSFullName |
| osReleaseType | GetOsReleaseType |
| displayVersion | GetDisplayVersion |
| distributionOSName | GetDistributionOSName |
| distributionOSVersion | GetDistributionOSVersion |
| distributionOSApiName | GetDistributionOSApiName |
| distributionOSReleaseType | GetDistributionOSReleaseType |

### 整数型字段（4 个）

| 字段名 | 对应接口 |
|--------|----------|
| sdkApiVersion | GetSdkApiVersion |
| sdkMinorApiVersion | GetSdkMinorApiVersion |
| sdkPatchApiVersion | GetSdkPatchApiVersion |
| distributionOSApiVersion | GetDistributionOSApiVersion |

## 命令列表

| 命令 | 说明 | 参数 | 权限 | 前置依赖 |
|------|------|------|------|----------|
| get | 查询单个设备信息字段 | `<field>`（字段名，必填） | 无 | 无 |
| all | 查询全部 11 个设备信息字段 | 无 | 无 | 无 |

**前置依赖说明**：
- **无**：该命令可直接执行，无需前置条件

## 示例

```bash
# 示例 1：查询 OS 全名（无前置依赖）
ohos-deviceInfo get osFullName

# 示例 2：查询显示版本号
ohos-deviceInfo get displayVersion

# 示例 3：查询 SDK API 版本号
ohos-deviceInfo get sdkApiVersion

# 示例 4：批量查询全部设备信息（无前置依赖）
ohos-deviceInfo all

# 示例 5：查看全量帮助
ohos-deviceInfo --help

# 示例 6：查看 get 子命令帮助
ohos-deviceInfo get --help

# 示例 7：查看 all 子命令帮助
ohos-deviceInfo all --help
```
