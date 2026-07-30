# DBService v1.4.1 编译说明

## 环境要求

- Qt 5.0+（推荐 Qt 5.12.x / 5.15.x）
- C++11 编译器（MinGW 或 MSVC）
- Qt SQL 模块 + 对应数据库驱动（如 qsqlmysql）

## 目录结构

```
DBService/
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

当前版本：`v1.4.1`，可通过 `DBService::currentVersion()` 在运行时获取。

## 更新说明

从 v1.4 升级到 v1.4.1 新增特性：

| 特性 | 说明 |
|------|------|
| BatchBuffer 共享指针 | 批量入队/出队路径 3 次深拷贝 → 1 次 |
| pendingTasks std::move | 每次任务完成省一次 SqlTuple 列表拷贝 |
| executeSqlUnit std::move | 每行数据少一次 QJsonObject 隐式共享 detach |
| Release 消除日志开销 | `QT_NO_DEBUG_OUTPUT` / `QT_NO_WARNING_OUTPUT` 编译时消除格式化 |

### v1.4.0 更新说明

| 特性 | 说明 |
|------|------|
| 合并信号 | 移除 `sigRawResult`，统一由 `sigExecFinished` 承载，减少跨线程开销 |
| rawJson 字段 | `DBServiceResult` 新增 `QJsonDocument rawJson`，零转换访问原始 JSON |
| extractData 按需调用 | 改为 `public static`，默认不执行，需要时手动调用 |
| toRawResult() | `DBServiceResult` 新增兼容方法，旧调用方一行迁移 |
| DBServiceRawResult 补齐 | 新增 `serviceName`/`errTupleIndex`，`errCode` 改为 `DBErrCode` 类型 |
| 内存优化 | TaskNode 共享指针、DBLogManager 直接读 rawJson |
