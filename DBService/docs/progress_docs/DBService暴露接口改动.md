# DBService 暴露接口改动

## 背景

当前 C/S 架构中，DBBridgeServer 调用 `DBService::onExecSqlList(sqlList)` 后无法将结果映射回具体的客户端请求。

### 问题链路

```
Client A (reqId_A) → DBBridgeServer → DBService::onExecSqlList(sqlList)
Client B (reqId_B) → DBBridgeServer → DBService::onExecSqlList(sqlList)

DBService 内部:
  onExecSqlList(sqlList) {
    DBTask task = DBTask::spawnRandomQUuidTask();  // 内部生成随机 taskId
    m_pendingTasks[task.taskId] = sqlList;          // taskId 外部不可知
    m_taskManager->pushTask(task);
  }

DBService 返回时:
  sigExecFinished(DBServiceResult) {
    taskId = 内部随机UUID         ← 外部无法预知
    sqlList = 原始SqlTuple列表    ← 唯一能用来匹配的字段
  }
```

**核心问题**：`DBService` 不提供让调用方指定 `taskId` 的入口，`DBServiceResult` 返回的内部随机 `taskId` 调用方无法预知，只能用 `sqlList` 内容硬匹配。

## 方案

给 `DBService` 新增一个 `onExecSqlList` 重载，让调用方传入自定义 `taskId`（如 IPC `requestId`），`DBServiceResult` 原样带回。

### 改动范围

**仅改 `DBService` 自身**，不涉及任何结构体。

| 文件 | 改动 |
|------|------|
| `DBService.h` | 新增 `void onExecSqlList(sqlList, taskId)` 声明 |
| `dbservice.cpp` | 新增实现（复用原逻辑，仅替换 taskId 生成方式） |

`DBTask`、`DBTaskResult`、`DBServiceStruct`、`DBTaskManager` **全都不动**。

### 具体改动

#### 1. `DBService.h`

```cpp
public slots:
    void onExecSqlList(const QVector<SqlTuple>& sqlList);
    void onExecSqlList(const QVector<SqlTuple>& sqlList, const QString& taskId);  // 新增
```

#### 2. `dbservice.cpp`

新增实现——与原始 `onExecSqlList` 唯一区别：不调用 `spawnRandomQUuidTask()`，直接用传入的 `taskId`：

```cpp
void DBService::onExecSqlList(const QVector<SqlTuple>& sqlList, const QString& taskId)
{
    if (!m_taskManager || sqlList.isEmpty()) {
        qWarning() << "DBService: TaskManager not initialized or empty sqlList";
        return;
    }

    // 使用调用方指定的 taskId（如 IPC requestId），不做随机生成
    DBTask task;
    task.taskId = taskId;
    task.requestTime = QDateTime::currentMSecsSinceEpoch();

    for (const SqlTuple& tuple : sqlList) {
        SqlUnit unit = SqlUnit::createSqlUnit(tuple.tag, tuple.sql, tuple.isModify);
        task.append(unit);
    }

    {
        QMutexLocker locker(&m_pendingMutex);
        m_pendingTasks[task.taskId] = sqlList;
    }

    if (m_threadModel == TaskThreadModel::WorkerThread) {
        QMetaObject::invokeMethod(m_taskManager, "onPushTask",
                                  Qt::QueuedConnection,
                                  Q_ARG(DBTask, task));
    } else {
        m_taskManager->onPushTask(task);
    }
}
```

`convertResult()` 无需修改——`DBServiceResult.taskId` 已经是从 `taskResult.task.taskId` 取出的，使用自定义 taskId 后会原样带回。

### 调用方使用方式（DBBridgeServer）

```cpp
// handleRequest 中
QString ipcRequestId = requestObj.value(IPCProtocol::KEY_REQUEST_ID).toString();
// ...
m_dbService->onExecSqlList(sqlTuples, ipcRequestId);  // 用 IPC requestId 作为 taskId

// trySendDbResult 中
if (result.taskId == m_pendingIpcRequestId) {
    // 精确匹配
}
```

### 优点

1. **改动极小**：只改 `DBService` 一个类，新增 1 个重载方法
2. **零结构体改动**：不新增任何字段，不影响 DLL ABI
3. **零侵入**：TaskManager 内部不感知任何变化
4. **匹配精确**：`result.taskId` 就是调用方传入的 `requestId`，一一对应
5. **保持兼容**：原 `onExecSqlList(sqlList)` 不变，不影响已有使用者
