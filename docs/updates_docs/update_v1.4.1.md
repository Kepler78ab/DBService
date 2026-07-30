# DBService v1.4.1 更新说明

> 版本：v1.4.1 | 日期：2026-07-28
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

### 移除

无

## 兼容性说明

- [x] **非破坏性变更** — 所有改动为内部实现优化，对外接口完全不变
- [ ] 业务项目不需要修改代码

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
| `docs/progress_docs/v1.4.1优化.md` | 新增 v1.4.1 优化设计文档 |
| `docs/updates_docs/update_v1.4.1.md` | 本文件 |
