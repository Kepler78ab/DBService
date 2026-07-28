# DBService v1.2 编译说明

## 环境要求

- Qt 5.0+（推荐 Qt 5.12.x / 5.15.x）
- C++11 编译器（MinGW 或 MSVC）
- Qt SQL 模块 + 对应数据库驱动（如 qsqlmysql）

## 目录结构

```
DBService_v1.2/
├── src/                    ← 源码
│   ├── DBService.pro
│   ├── DBService/          ← DBService 模块
│   ├── DbAccess/           ← 数据库访问层
│   └── dbservice_global.h
├── include/                ← 对外头文件
├── docs/                   ← 文档
└── build/
    ├── debug/              ← Debug 编译输出
    └── release/            ← Release 编译输出
```

## 编译步骤

```bash
# 进入源码目录
cd DBService_v1.2/src

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

```
Release:
  build/release/
  ├── DBService.dll      ← 运行时 DLL
  └── libDBService.a     ← 导入库（MSVC 下为 DBService.lib）

Debug:
  build/debug/
  ├── DBService.dll      ← 运行时 DLL（链接 Qt*_debug 库）
  └── libDBService.a     ← 导入库
```

## 对外接口

编译后将 `include/` 目录和对应模式的 DLL 提供给业务项目使用：

```
include/
├── DBService.h            ← 公共 API（含使用说明和线程模式说明）
├── DBLogManager.h         ← 日志管理器
├── DBServiceStruct.h      ← 数据结构体
├── DBConfig.h             ← DB配置结构体
└── dbservice_global.h     ← DLL 导入/导出宏
```

## 版本号

当前版本：`v1.2.0`，可通过 `DBService::currentVersion()` 在运行时获取。

## 更新说明

从 v1.1 升级到 v1.2 新增特性：

| 特性 | 说明 |
|------|------|
| 线程模式选择 | 新增 `TaskThreadModel` 枚举，构造函数增加 `model` 参数 |
| MainThread（默认） | 旧行为，TaskManager 在主线程，零改动升级 |
| WorkerThread | TaskManager 移入子线程，SQL 阻塞不卡 UI |
| qRegisterMetaType | `DBTask`、`DBTaskResult`、`DBServiceRawResult` 已注册，跨线程信号正常工作 |
| Debug/Release 分离 | 编译产物按模式分别输出，避免混合链接问题 |
| 自定义 taskId | 新增 `onExecSqlList(sqlList, taskId)` 重载，用于请求追踪精确匹配 |
| 原始 JSON 信号 | 新增 `sigRawResult(const DBServiceRawResult&)` 信号，直传 `QJsonObject` 零转换 |
| DBServiceRawResult | 新增结构体，含 `taskId/QJsonObject/isSuccess/errCode/errMsg/errSql` |
| Q_INVOKABLE | `DBTaskManager::start()` 和 `DBTaskManager::onPushTask()` 增加 `Q_INVOKABLE`，支持跨线程 invokeMethod |
