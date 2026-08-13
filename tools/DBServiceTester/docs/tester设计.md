# DBServiceTester 测试设计文档

> 版本：v1.0 | 日期：2026-07-28
> 本文档描述 DBServiceTester 项目的设计目标、测试范围、架构方案和界面规划。

---

## 一、项目目标

构建一个基于 Qt Widgets 的图形化测试工具，对 `DBService`（v1.3.0）DLL 进行全面的功能验证和可视化测试。

### 核心目标

1. **功能验证**：覆盖 DBService 暴露的所有公开接口（构造、初始化、SQL 执行、信号、配置文件）
2. **场景覆盖**：4 种服务类型 × 2 种线程模式 × 多种 SQL 类型的组合测试
3. **边界测试**：空参数、高频调用、大结果集、无连接、析构中任务等异常场景
4. **未来扩展预留**：超时检测等未来功能的一键启用/测试

---

## 二、测试范围

### 2.1 当前功能测试（v1.3.0）

| 测试模块 | 测试项 | 优先级 |
|---------|-------|-------|
| **构造与初始化** | 3 种构造函数 | P0 |
| | 4 种服务类型（Reliable / HighPerf / Standard / Unreliable） | P0 |
| | 2 种线程模式（MainThread / WorkerThread） | P0 |
| | 重复 init、空构造后延迟 init | P1 |
| | init 失败（无数据库）仍返回 true 的行为验证 | P1 |
| **SQL 执行** | SELECT 查询 | P0 |
| | INSERT / UPDATE / DELETE 修改操作 | P0 |
| | 多条 SqlTuple 混合执行 | P0 |
| | 自定义 taskId 投递与匹配（`onExecSqlList(sqlList, taskId)`） | P0 |
| | 空 sqlList 边界 | P1 |
| | 语法错误 / 表不存在等错误 SQL | P1 |
| **信号验证** | `sigExecFinished` 字段完整性（isSuccess / taskId / errCode / data） | P0 |
| | `sigRawResult` 原始 JSON 直传验证（与 sigExecFinished 数据一致性） | P0 |
| | 两路信号是否同时正确触发 | P0 |
| | taskId 请求-响应一一对应 | P0 |
| **配置文件** | `loadSimpleConfig` 加载完整配置 | P0 |
| | 各 driverType 字符串解析 | P1 |
| | 配置字段缺失时默认值生效 | P1 |
| | 手动构造 DBConfig 并 init | P1 |
| **日志模块** | DBLogManager start/stop | P1 |
| | 日志文件生成与 JSON Lines 格式 | P1 |
| | serviceName 分组写入 | P1 |
| **服务类型行为** | Reliable：断线后自动重试，最终执行成功 | P0 |
| | HighPerf：批量入队时机和合并行为 | P1 |
| | Standard：有限重试次数后通知失败 | P1 |
| | Unreliable：NoRetry，失败立即通知 | P1 |
| **线程模式** | MainThread：SQL 执行阻塞主线程（观察 UI 响应） | P1 |
| | WorkerThread：SQL 执行不阻塞 UI | P0 |
| | 跨线程信号正常送达（QueuedConnection） | P1 |
| | 析构时 WorkerThread 正确清理 | P1 |
| **边界/压力** | 连续发送 100+ 条任务（队列满 REJECT 行为） | P1 |
| | 无数据库连接时投递 SQL | P1 |
| | 大结果集返回（万行级别） | P1 |
| | DBService 析构时仍有 pending 任务 | P2 |

### 2.2 未来功能测试预留（v1.4+）

| 测试项 | 状态 |
|-------|------|
| 超时任务触发 `sigTaskTimedOut` | 预留 |
| 超时后事务 rollback，返回 `TASK_EXPIRED` | 预留 |
| `DBServiceResult::isTimeout` 字段验证 | 预留 |

---

## 三、架构设计

### 3.1 目录结构

```
DBServiceTester/
├── src/                          # Qt 项目源码
│   ├── DBServiceTester.pro       # 构建工程
│   ├── main.cpp                  # 入口
│   ├── MainWindow.h/.cpp         # 主窗口
│   ├── Widgets/                  # 界面组件
│   │   ├── ConnectionPanel.h/.cpp    # 连接配置面板
│   │   ├── SqlInputPanel.h/.cpp      # SQL 输入面板
│   │   ├── ResultPanel.h/.cpp        # 结果展示面板
│   │   └── ConfigPanel.h/.cpp        # 服务配置面板
│   └── TestCases/                # 测试用例（可选，简化版直接 UI 操作）
│       └── TestRunner.h/.cpp
├── include/                      # DBService 对外头文件（拷贝自 DBService/include/）
│   ├── DBService.h
│   ├── DBLogManager.h
│   ├── DBServiceStruct.h
│   ├── DBConfig.h
│   └── dbservice_global.h
├── lib/
│   ├── debug/                    # Debug DLL 导入库
│   │   ├── DBService.dll
│   │   └── libDBService.a
│   └── release/                  # Release DLL 导入库
│       ├── DBService.dll
│       └── libDBService.a
├── config/                       # 测试配置文件
│   ├── DBConfig.ini
│   └── LogConfig.ini
├── docs/
│   └── tester设计.md              # 本文档
└── build/                        # 编译输出
    ├── debug/
    └── release/
```

### 3.2 界面布局

```
┌──────────────────────────────────────────────────────────────┐
│  DBServiceTester v1.0                        服务状态: ● 已启动 │
├───────────────────────┬──────────────────────────────────────┤
│  连接配置              │  SQL 输入                            │
│  ┌─────────────────┐  │  ┌────────────────────────────────┐  │
│  │ 主机: [127.0.0.1]│  │  │ SELECT * FROM users           │  │
│  │ 端口: [3306]     │  │  │ INSERT INTO log VALUES(...)   │  │
│  │ 库名: [mydb]     │  │  │ ...                           │  │
│  │ 用户: [root]     │  │  └────────────────────────────────┘  │
│  │ 密码: [*****]    │  │  [+ 添加] [✕ 移除] [⚡ 执行]       │
│  └─────────────────┘  │                                        │
│                        │  SqlTuple 列表:                        │
│  服务配置              │  ┌─[✓] SELECT ... [isModify: ✗]──┐  │
│  ┌─────────────────┐  │  │─[✓] INSERT ... [isModify: ✓]──│  │
│  │ 服务类型: [▼]    │  │  └────────────────────────────────┘  │
│  │ 线程模式: [▼]    │  │                                        │
│  │ 启用超时: [✗]    │  │                                        │
│  │ taskId: [auto/   │  │                                        │
│  │         custom]  │  │                                        │
│  └─────────────────┘  │                                        │
├───────────────────────┴──────────────────────────────────────┤
│  结果日志                                                    │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ [INFO] DBService v1.3.0 initialized, type=Reliable     │  │
│  │ [OK]   taskId=abc-123  | 3 rows returned  | 12ms       │  │
│  │ [ERR]  taskId=abc-456  | SQL syntax error              │  │
│  │ [RAW]  taskId=abc-123  | {"users": [...]}              │  │
│  └────────────────────────────────────────────────────────┘  │
│  [清除日志] [保存日志] [连接] [断开]                           │
└──────────────────────────────────────────────────────────────┘
```

### 3.3 模块职责

| 模块 | 职责 |
|------|------|
| **MainWindow** | 主窗口，管理各面板协作、DBService 生命周期、信号连接 |
| **ConnectionPanel** | 数据库连接配置输入（主机/端口/库名/用户/密码/驱动类型） |
| **ConfigPanel** | 服务类型选择、线程模式、超时开关、自定义 taskId |
| **SqlInputPanel** | SQL 语句输入（支持添加/删除多条）、批量执行 |
| **ResultPanel** | 执行结果日志展示（区分信息/成功/错误/原始 JSON 等） |

---

## 四、界面功能设计

### 4.1 连接配置面板（ConnectionPanel）

- 输入字段：主机、端口、库名、用户、密码
- 驱动类型下拉：MySQL / Oracle / ODBC / PostgreSQL / SQLite / DB2
- 操作按钮：[加载配置] [连接测试] [应用]

### 4.2 服务配置面板（ConfigPanel）

- 服务类型下拉：Reliable / HighPerf / Standard / Unreliable
- 线程模式下拉：MainThread（默认） / WorkerThread
- 超时启用复选框 + 超时时间输入（毫秒），默认不启用
- taskId 模式：Auto（随机UUID）/ Custom（手动输入）

### 4.3 SQL 输入面板（SqlInputPanel）

- 多行 SQL 输入框，支持输入多条语句
- 每条语句可单独标记 isModify（勾选表示 INSERT/UPDATE/DELETE）
- [添加] 按钮：将当前 SQL 加入待执行列表
- [移除] 按钮：从列表中移除选中行
- [执行] 按钮：将列表中的 SQL 打包为 SqlTuple 向量投递
- 显示当前待执行 SQL 列表（带序号、SQL 内容、isModify 标记）

### 4.4 结果展示面板（ResultPanel）

- QPlainTextEdit 只读展示
- 每条日志带时间戳和类型前缀：
  - `[INFO]` — 系统信息（版本、初始化等）
  - `[OK]` — 执行成功（显示 taskId、行数、耗时）
  - `[ERR]` — 执行失败（显示错误码、错误信息、错误 SQL）
  - `[RAW]` — 原始 JSON 结果
  - `[WARN]` — 警告信息
- 按钮：[清除日志] [保存日志到文件]

### 4.5 状态栏

- DBService 当前状态（未初始化 / 已启动 / 已停止）
- 当前服务类型和线程模式

---

## 五、关键测试场景

### 5.1 基础测试流程

1. 配置数据库连接 → [应用]
2. 选择服务类型和线程模式 → 创建 DBService
3. [连接] → DBService::start()
4. 输入 SQL → [添加] → [执行] → 观察结果
5. 切换服务类型/线程模式，重复测试

### 5.2 自定义 taskId 测试

1. 在 ConfigPanel 中勾选 "自定义 taskId"
2. 输入自定义 ID（如 `"test_req_001"`）
3. 执行 SQL
4. 验证 sigExecFinished 返回的 `result.taskId` == `"test_req_001"`

### 5.3 原始 JSON 信号验证

1. 执行 SELECT 查询
2. 分别接收 `sigExecFinished` 和 `sigRawResult`
3. 对比 `result.data`（QVector<QVariantMap>）与 `rawResult.resultJson`（QJsonObject）
4. 用 `QJsonValue::fromVariant` 将 `result.data` 转 JSON，与 `rawResult` 对比差异

### 5.4 服务类型行为验证

1. 用 Unreliable 类型连接不存在的表 → 应快速返回失败
2. 用 Reliable 类型在数据库关闭时执行 → 应自动重试（观察日志中的多次尝试）
3. 用 HighPerf 类型连续投递多条 SQL → 观察批量入队行为

### 5.5 WorkerThread 非阻塞验证

1. 选择 WorkerThread 模式
2. 执行一个耗时 SQL（如 `SELECT SLEEP(5)`）
3. 验证 UI 在 SQL 执行期间仍可操作（拖动、点击等）

---

## 六、 .pro 文件构建要求

```pro
TEMPLATE = app
TARGET = DBServiceTester
QT = core sql widgets

CONFIG += c++11

# DBService 对外头文件路径
INCLUDEPATH += $$PWD/../include

# Debug/Release 分别链接 DBService DLL
CONFIG(debug, debug|release) {
    LIBS += -L$$PWD/../lib/debug -lDBService
} else {
    LIBS += -L$$PWD/../lib/release -lDBService
}

# 运行时 DLL 拷贝到输出目录
CONFIG(debug, debug|release) {
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$PWD/../lib/debug/DBService.dll) $$shell_path($$OUT_PWD/debug/) $$escape_expand(\\n\\t)
} else {
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$PWD/../lib/release/DBService.dll) $$shell_path($$OUT_PWD/release/) $$escape_expand(\\n\\t)
}

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    Widgets/ConnectionPanel.cpp \
    Widgets/ConfigPanel.cpp \
    Widgets/SqlInputPanel.cpp \
    Widgets/ResultPanel.cpp

HEADERS += \
    MainWindow.h \
    Widgets/ConnectionPanel.h \
    Widgets/ConfigPanel.h \
    Widgets/SqlInputPanel.h \
    Widgets/ResultPanel.h
```

---

## 七、未来扩展

| 版本 | 新增内容 |
|------|---------|
| v1.1 | 超时检测测试（启用/禁用、超时信号、超时回滚验证） |
| v1.2 | 测试用例自动化（预设测试用例，一键批量运行） |
| v1.3 | 结果对比功能（不同服务类型执行同一 SQL 的结果差异） |
