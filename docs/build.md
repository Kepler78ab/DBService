# DBService v1.5.1 编译说明

## 环境要求

- Qt 5.12.3+（推荐 Qt 5.12.x / 5.15.x）
- C++11 编译器（MinGW 8.1.0 / MSVC 2017）
- Qt SQL 模块 + 对应数据库驱动（如 qsqlmysql）

## 目录结构

```
DBService/
├── src/                    ← 源码
│   ├── DBService.pro
│   ├── version_info.rc     ← DLL 版本资源（1.5.1.0）
│   ├── DBService/          ← DBService 模块
│   ├── DBServicePool/      ← DBServicePool 拓展模块（v1.5.1）
│   ├── DbAccess/           ← 数据库访问层
│   └── dbservice_global.h
├── include/                ← 对外头文件
├── docs/                   ← 文档
└── lib/
    ├── debug/              ← Debug 编译输出
    └── release/            ← Release 编译输出
```

## 编译步骤

```bash
# 进入源码目录
cd DBService/src

# 生成 Makefile
qmake DBService.pro

# 编译 Debug
qmake CONFIG+=debug DBService.pro
mingw32-make

# 编译 Release
qmake CONFIG+=release DBService.pro
mingw32-make

# 清理
mingw32-make clean
```

## 编译产物

> Debug 和 Release 使用不同 DLL 名称，避免混用。

```
Release:
  lib/release/
  ├── DBService.dll        ← 运行时 DLL
  └── libDBService.a       ← 导入库（MinGW）/ DBService.lib（MSVC）

Debug:
  lib/debug/
  ├── DBServiced.dll       ← 运行时 DLL（d 后缀，链接 Qt*_debug 库）
  └── libDBServiced.a      ← 导入库（MinGW）/ DBServiced.lib（MSVC）
```

DLL 右键属性 → 详细信息 → 产品版本 `1.5.1`（由 `version_info.rc` 嵌入）。

## 对外接口

编译后将 `include/` 目录和对应模式的 DLL 提供给业务项目使用：

```
include/
├── DBService.h            ← 公共 API（含线程模式说明）
├── DBServicePool.h        ← 连接池 API（v1.5.1 新增）
├── DBLogManager.h         ← 日志管理器
├── DBServiceStruct.h      ← 数据结构体
├── DBConfig.h             ← DB配置结构体
└── dbservice_global.h     ← DLL 导入/导出宏
```

## 版本号

当前版本：`v1.5.1`，可通过 `DBService::currentVersion()` 在运行时获取。