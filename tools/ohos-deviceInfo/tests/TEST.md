# ohos-deviceInfo 测试用例

本文件覆盖 `ohos-deviceInfo` 的全部 2 个子命令（`get`、`all`）的所有参数组合，包括 11 个字段的 `get` 查询、`all` 批量查询、`--help` 帮助及错误场景。权限列填充「无」（CLI 不涉及任何 `ohos.permission` 权限），前置依赖列填充「无」（所有命令均可独立执行）。

## get 命令（字符串型字段）

| 命令示例 | 说明 | 权限 | 前置依赖 |
|------|------|------|----------|
| ohos-deviceInfo get osFullName | 查询 OS 全名 | 无 | 无 |
| ohos-deviceInfo get osReleaseType | 查询 OS 发布类型 | 无 | 无 |
| ohos-deviceInfo get displayVersion | 查询显示版本号 | 无 | 无 |
| ohos-deviceInfo get distributionOSName | 查询发行版 OS 名称 | 无 | 无 |
| ohos-deviceInfo get distributionOSVersion | 查询发行版 OS 版本 | 无 | 无 |
| ohos-deviceInfo get distributionOSApiName | 查询发行版 OS API 名称 | 无 | 无 |
| ohos-deviceInfo get distributionOSReleaseType | 查询发行版 OS 发布类型 | 无 | 无 |

## get 命令（整数型字段）

| 命令示例 | 说明 | 权限 | 前置依赖 |
|------|------|------|----------|
| ohos-deviceInfo get sdkApiVersion | 查询 SDK API 版本号 | 无 | 无 |
| ohos-deviceInfo get sdkMinorApiVersion | 查询 SDK 次版本号 | 无 | 无 |
| ohos-deviceInfo get sdkPatchApiVersion | 查询 SDK 补丁版本号 | 无 | 无 |
| ohos-deviceInfo get distributionOSApiVersion | 查询发行版 OS API 版本号 | 无 | 无 |

## all 命令

| 命令示例 | 说明 | 权限 | 前置依赖 |
|------|------|------|----------|
| ohos-deviceInfo all | 批量查询全部 11 个设备信息字段 | 无 | 无 |

## help 命令

| 命令示例 | 说明 | 权限 | 前置依赖 |
|------|------|------|----------|
| ohos-deviceInfo --help | 查看全量帮助，列出所有子命令 | 无 | 无 |
| ohos-deviceInfo get --help | 查看 get 子命令详细帮助 | 无 | 无 |
| ohos-deviceInfo all --help | 查看 all 子命令详细帮助 | 无 | 无 |
| ohos-deviceInfo help | 查看全量帮助 | 无 | 无 |
| ohos-deviceInfo help get | 查看 get 子命令详细帮助 | 无 | 无 |
| ohos-deviceInfo help all | 查看 all 子命令详细帮助 | 无 | 无 |

## 错误场景

| 命令示例 | 说明 | 权限 | 前置依赖 |
|------|------|------|----------|
| ohos-deviceInfo | 无子命令，打印用法提示 | 无 | 无 |
| ohos-deviceInfo unknownCmd | 未知子命令，返回错误 | 无 | 无 |
| ohos-deviceInfo get | 缺少必填参数 `<field>`，返回 ERR_ARG_MISSING | 无 | 无 |
| ohos-deviceInfo get unknownField | 未知字段名，返回 ERR_ARG_INVALID | 无 | 无 |
