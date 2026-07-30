# DBService v1.4.0 更新说明

> 版本：v1.4.0 | 日期：2026-07-28
> 本版本在 v1.3.0（自定义 taskId + 原始 JSON 信号）基础上，优化大数据量场景下的内存和 CPU 占用。

---

## 更新类型

- [ ] 修复（bug fix）
- [x] 功能（feature）
- [x] 重构（refactor）
- [ ] 文档（docs）
- [ ] 破坏性变更（breaking）

## 变更内容

### 新增

1. **`DBServiceResult::rawJson` 字段**
   - `DBServiceResult` 新增 `QJsonDocument rawJson`，携带原始查询结果 JSON
   - `sigExecFinished` 默认不再调用 `extractData`，`data` 字段为空
   - 需要结构化数据时，调用方可按需调用 `DBService::extractData(result.rawJson)`

2. **`DBServiceResult::toRawResult()` 兼容方法**
   - 返回 `DBServiceRawResult`，方便旧调用方一行迁移
   - 无需修改回调内部逻辑

3. **`DBService::extractData` 改为 `public static`**
   - 参数从 `const DBTaskResult&` 改为 `const QJsonDocument&`
   - 调用方按需手动调用，不再 DLL 内部强制转换

4. **`DBServiceRawResult` 补齐字段**
   - 新增 `serviceName`、`errTupleIndex` 字段
   - `errCode` 从 `int` 改为 `DBErrCode` 枚举类型
   - `resultJson` 从 `QJsonObject` 改为 `QJsonDocument`（零拷贝传递）

5. **`Q_DECLARE_METATYPE(DBServiceResult)`**
   - 注册元类型，确保 WorkerThread 模式下跨线程信号正常工作

### 修复

无

### 变更

6. **合并信号：移除 `sigRawResult`**
   - 不再发射单独的 `sigRawResult` 信号，统一由 `sigExecFinished` 承载
   - WorkerThread 模式下减少一次跨线程 QueuedConnection 开销

7. **`convertResult` 不再调用 `extractData`**
   - 原始 JSON 通过 `rawJson` 字段直接传递，零转换
   - `sigExecFinished` 的 `data` 字段默认为空

8. **`TaskNode::DBTask` 改为 `std::shared_ptr<DBTask>`**
   - `dbtaskmanager.h` 中 `TaskNode` 结构的 `task` 字段从值类型改为共享指针
   - 任务入队、出队、重试路径省去 3~5 次 DBTask 深拷贝

9. **`DBLogManager::buildJsonLine` 改读 `rawJson`**
   - 从 `result.data`（QVariantMap）往返转换改为直接取 `result.rawJson.object()`
   - 消除无用 QVariantMap → QJsonObject 转换

10. **版本号更新**
    - `currentVersion()` 返回 `"v1.4.0"`

### 移除

- 移除 `sigRawResult(const DBServiceRawResult&)` 信号声明和发射
- 移除 `extractData` 私有方法的 `const DBTaskResult&` 重载

## 兼容性说明

- [x] **非破坏性变更** — 所有改动为新增/变更，无接口直接移除（`sigRawResult` 移除前已通过 `toRawResult()` 提供迁移路径）
- [ ] 业务项目可能需要修改代码（见下方迁移指南）

## 迁移指南

### 从 v1.3.0 迁移到 v1.4.0

#### 使用 `sigRawResult` 的项目

```cpp
// 旧
connect(db, &DBService::sigRawResult, this, [](const DBServiceRawResult& raw) {
    sendToWebSocket(raw.resultJson);
});

// 新：connect 改连 sigExecFinished，回调内加一行 toRawResult
connect(db, &DBService::sigExecFinished, this, [](const DBServiceResult& res) {
    auto raw = res.toRawResult();     // 兼容层，拿到完全相同的格式
    sendToWebSocket(raw.resultJson);  // 回调内一行不改
});
```

#### 使用 `sigExecFinished` + `data` 的项目

```cpp
// 旧：data 自动填充
auto rows = result.data["users"].value<QVector<QVariantMap>>();

// 新：按需手动调用 extractData
auto data = DBService::extractData(result.rawJson);
auto rows = data["users"].value<QVector<QVariantMap>>();
```

#### 不需要结构化数据的项目（推荐）

```cpp
// 直接使用 rawJson，零转换
connect(db, &DBService::sigExecFinished, this, [](const DBServiceResult& res) {
    QByteArray json = res.rawJson.toJson(QJsonDocument::Compact);
    socket->send(json);
});
```

## 影响范围

| 文件 | 改动类型 |
|------|---------|
| `include/DBServiceStruct.h` | `DBServiceResult` 新增 `rawJson`/`toRawResult()`，`DBServiceRawResult` 补齐字段+QJsonDocument |
| `include/DBService.h` | 移除 `sigRawResult`，`extractData` 改为 public static |
| `src/DBService/dbservicestruct.h` | 同步外部结构体改动 |
| `src/DBService/dbservice.h` | 同步外部接口改动 |
| `src/DBService/dbservice.cpp` | 移除 sigRawResult emit，convertResult 不调 extractData，实现 public static extractData |
| `src/DBService/DBLogManager.cpp` | buildJsonLine 改读 rawJson |
| `src/DbAccess/DBTaskManager/dbtaskmanager.h` | TaskNode::task 改为 shared_ptr |
| `src/DbAccess/DBTaskManager/dbtaskmanager.cpp` | 适配 shared_ptr 的 -> 访问和 make_shared 构造 |
| `docs/progress_docs/内存优化.md` | 新增内存优化设计文档 |
| `docs/updates_docs/update_v1.4.0.md` | 本文件 |
