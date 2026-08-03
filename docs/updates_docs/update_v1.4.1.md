# DBService v1.4.1 更新说明

> 版本：v1.4.1 | 日期：2026-07-28（2026-08-03 补充日志打点与结果路径评估）
> 本版本在 v1.4.0（合并信号 + 内存优化）基础上，进一步消除剩余深拷贝和运行时开销。

---

## 更新类型

- [ ] 修复（bug fix）
- [x] 功能（feature）
- [x] 重构（refactor）
- [ ] 文档（docs）
- [ ] 破坏性变更（breaking）

## 变更内容

### 新增

无

### 修复

无

### 变更

1. **BatchBuffer 改为共享指针**
   - `m_batchBuffer` 类型从 `QQueue<DBTask>` 改为 `QQueue<std::shared_ptr<DBTask>>`
   - 批量入队/出队路径从 3 次 `QVector<SqlUnit>` 深拷贝（enqueue → dequeue → make_shared）降为 1 次（make_shared 构造）
   - Standard/HighPerf 服务类型受益，`flushBatchBuffer` 出队时零拷贝传递到 TaskNode

2. **pendingTasks 提取加 std::move**
   - `dbservice.cpp` 中 `originalTupleList = it.value()` → `originalTupleList = std::move(it.value())`
   - 每次任务完成时省去一次 `QVector<SqlTuple>` 深拷贝

3. **executeSqlUnit 行追加加 std::move**
   - `qdbconnection.cpp` 中 `resultArray.append(obj)` → `resultArray.append(std::move(obj))`
   - 每行数据省一次 QJsonObject 隐式共享 detach，大数据量场景收益显著

4. **Release 编译消除调试日志开销**
   - `DBService.pro` 在 Release 配置下添加 `DEFINES += QT_NO_DEBUG_OUTPUT QT_NO_WARNING_OUTPUT`
   - 所有 `qDebug()` / `qWarning()` 编译为空操作，运行时零格式化开销

5. **版本号同步**
   - `.pro` 版本号 1.3.0 → 1.4.1
   - `currentVersion()` 返回 `"v1.4.1"`

6. **单 SQL 单元结果路径评估（2026-08-03，未引入接口变更）**
   - 评估结论：`QJsonValue` 构造 QJsonArray 参数为引用计数共享（O(1)），`resultObj[unit.jsonKey] = resultArray` 并非深拷贝；链路中唯一深拷贝为 `QJsonDocument(resultObj)` 构造
   - 曾评估新增 `QJsonArray resultArray` 字段以消除该次拷贝（峰值 ~190MB → ~130MB），但会破坏接口稳定性、要求调用方重编译
   - **决策：放弃新增字段，保持对外接口完全不变**；深拷贝发生在工作线程且可正常释放，非泄漏
   - 保留 `executeSqlUnit` 的 `std::move`（第 3 条）等内部优化

7. **运行链路日志打点**（2026-08-03 补充）
   - `[DBService]`：收到请求（taskId / sqlCount / totalLen）、回调（taskId / isSuccess / rows / elapsed ms）
   - `[DBTaskManager]`：任务入队（taskId / queueDepth）
   - `[QDBConnection]`：执行开始（taskId / unitCount）、任务完成（taskId / isSuccess / rows / elapsed ms）
   - 用于请求时序对账与内存/耗时排查；注意：Release 下若同时定义 `QT_NO_DEBUG_OUTPUT` + `QT_NO_WARNING_OUTPUT`，Qt 5.12 会将 `qInfo` 一并编译为空操作（见第 4 条）

8. **核心结构体字段重排，减少 padding**（2026-08-03 补充）
   - 仅调整字段声明顺序（8 字节成员在前、小字段集中尾部），函数体、字段名、枚举值均未变
   - `DBConfig`：64 → **48** 字节（省 16B）
   - `DBServiceResult`：80 → **72** 字节（省 8B）
   - `DBTaskResult`（内部）：80 → **72** 字节（省 8B）
   - `DBTaskManagerConfig`（内部）：104 → **88** 字节（省 16B）
   - 未改动（已最优）：`SqlTuple`/`SqlUnit`/`DBTask`/`TaskNode`/`DBServiceRawResult`
   - 构造函数初始化列表顺序同步调整以匹配新声明顺序（无行为变化）

### 移除

无

## 兼容性说明

- [x] **接口（函数签名）不变** — 无新增/删除/变更 API，业务代码**无需修改**
- [x] **结构体布局变化（需重编译）** — `DBConfig`/`DBServiceResult` 字段顺序调整，调用方须用新头文件重新编译（按字段名访问不受影响）

## 迁移指南

无，内部优化，调用方零改动。

## 影响范围

| 文件 | 改动类型 |
|------|---------|
| `src/DbAccess/DBTaskManager/dbtaskmanager.h` | `m_batchBuffer` 类型 `QQueue<DBTask>` → `QQueue<std::shared_ptr<DBTask>>` |
| `src/DbAccess/DBTaskManager/dbtaskmanager.cpp` | 批量入队/出队适配 shared_ptr 模式 |
| `src/DBService/dbservice.cpp` | pendingTasks 提取加 `std::move`；`currentVersion()` → v1.4.1 |
| `src/DbAccess/QDBConnection/qdbconnection.cpp` | resultArray.append 加 `std::move` |
| `src/DBService.pro` | Release 加 `QT_NO_DEBUG_OUTPUT` / `QT_NO_WARNING_OUTPUT`；版本号 1.3.0 → 1.4.1 |
| `include/DBConfig.h` | `DBConfig` 字段重排（64 → 48B） |
| `src/DbAccess/DbAccessStruct/dbconfig.h` | `DBConfig` 字段重排（64 → 48B） |
| `include/DBServiceStruct.h` | `DBServiceResult` 字段重排（80 → 72B） |
| `src/DBService/dbservicestruct.h` | `DBServiceResult` 字段重排（80 → 72B） |
| `src/DbAccess/DbAccessStruct/dbtaskresult.h` | `DBTaskResult` 字段重排（80 → 72B） |
| `src/DbAccess/DBTaskManager/dbtaskmanager.h` | `DBTaskManagerConfig` 字段重排（104 → 88B） |
| `src/DBService/dbservice.cpp` | 收到请求/回调日志打点（rows 由 `countResultRows` 统计） |
| `src/DbAccess/QDBConnection/qdbconnection.cpp` | 执行开始/完成日志打点 |
| `src/DbAccess/DBTaskManager/dbtaskmanager.cpp` | 任务入队日志打点 |
| `docs/progress_docs/v1.4.1优化.md` | 新增 v1.4.1 优化设计文档 |
| `docs/updates_docs/update_v1.4.1.md` | 本文件 |
