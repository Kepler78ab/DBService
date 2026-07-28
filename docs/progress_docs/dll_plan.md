# DBService DLL 化方案（v1.1）

## 接口边界

**对外暴露（include/）**

| 头文件 | 内容 |
|--------|------|
| DBService.h | 公共 API：构造、init、start、onExecSqlList、currentVersion |
| DBLogManager.h | 日志管理器：start、stop、LogConfig::loadFromIni |
| DBServiceStruct.h | SqlTuple、DBServiceResult、DBServiceType |
| DBConfig.h | DBConfig、DBErrCode、DBDriverType |
| dbservice_global.h | DBSERVICE_EXPORT 导入/导出宏 |

**内部隐藏（不对外暴露）**

| 模块 | 说明 |
|------|------|
| QDBConnection | 数据库连接管理 |
| DBTaskManager | 任务调度 |
| DBTaskResult | 内部任务结果 |
| DBTaskManagerConfig | 任务调度配置 |

## 配置边界

```
config/
├── DBConfig.ini    ← [Database] + [Service]
└── LogConfig.ini   ← [Log]
```

## DLL 产物结构

```
DBService_v1.1/
├── src/                   ← 源码目录
│   ├── DBService.pro
│   ├── DBService/         ← 服务模块
│   ├── DbAccess/          ← 数据库访问层
│   └── dbservice_global.h
├── include/               ← 对外头文件
├── docs/                  ← 文档
└── build/release/
    ├── DBService.dll      ← DLL 产物
    └── libDBService.a     ← 导入库
```

## 业务项目引用方式

```pro
INCLUDEPATH += $$PWD/include
LIBS += -L$$PWD/lib -lDBService
```

运行时 `DBService.dll` 放在 exe 同级目录。

## 版本策略

| 版本 | 说明 |
|------|------|
| v1.1.x | 内部 bug 修复，接口不变 |
| v1.2.x | 兼容新增能力 |
| v2.0.x | 破坏性变更 |

## 编译环境

- Qt 5.15.2
- MinGW 8.1.0
- C++11
