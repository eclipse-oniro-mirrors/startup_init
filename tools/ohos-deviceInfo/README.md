# ohos-deviceInfo

OpenHarmony 设备信息查询 CLI 工具，封装 `libbegetutil` 的 11 个设备信息 getter 接口（7 个字符串型 + 4 个整数型），提供 `get` 和 `all` 两个子命令。

## 目录结构

```
tools/ohos-deviceInfo/
  ├── src/
  │   ├── main.cpp              # 主入口，参数解析，get/all 子命令分发
  │   ├── string_getters.h      # 字符串型 getter 声明（28 个）
  │   ├── string_getters.cpp    # 字符串型 getter 实现
  │   ├── int_getters.h         # 整数型 getter 声明（8 个）
  │   ├── int_getters.cpp       # 整数型 getter 实现
  │   ├── field_registry.h      # 字段名→getter 映射声明
  │   ├── field_registry.cpp    # 字段注册表实现（36 条映射）
  │   ├── output_formatter.h    # 输出格式化声明（CLI_LOG/CLI_ERROR/OutputSuccess/OutputError）
  │   └── output_formatter.cpp  # 输出格式化实现
  ├── tests/
  │   └── TEST.md               # 测试文档
  ├── docs/
  │   └── README.md             # 工具使用说明
  ├── BUILD.gn                  # GN 构建配置
  ├── config.json               # 工具描述文件
  └── README.md                 # 项目说明
```

## 构建

```bash
# 在 OpenHarmony 源码树根目录
./build.sh --product-name <product> --build-target ohos-deviceInfo
```

安装路径：`/system/bin/cli_tool/executable/ohos-deviceInfo`

## 用法

```bash
# 查询单个字段
ohos-deviceInfo get osFullName

# 查询全部字段
ohos-deviceInfo all

# 查看帮助
ohos-deviceInfo --help
```

详见 [docs/README.md](docs/README.md)。
