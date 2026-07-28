# DBService 添加超时处理改造

## 一、需求与原因

### 1.1 需求
DBService 当前缺少任务超时检测机制。当 SQL 执行时间过长（网络抖动、锁等待、慢查询），DBService 会一直阻塞直到数据库返回，无法告知调用方"这个请求已超时"。

### 1.2 原因
DBService 作为通用数据库服务 DLL，需要被桥接到框架（DBBridge）使用时，框架依赖超时机制来：
- 避免前端 UI 无限等待
- 保证超时后 rollback，不提交过期数据
- 配合 DBBridge 的路由表，超时后释放路由资源

### 1.3 设计目标
1. **默认不启用**，不影响现有使用者
2. **超时时间默认 5000ms**，可通过配置文件项启用/禁用、配置超时值
3. **WorkerThread 模式下，超时检测不影响主线程事件循环**
4. **超时后回滚事务**，不提交部分执行的 SQL
5. **SQL 长时间阻塞 QDBConnection 时的行为说明**

---

## 二、方案

### 2.1 整体架构

```
DBService::onExecSqlList(sqlList)
  │
  ├── 创建 DBTask（taskId + requestTime）
  │
  ├── [新增] 如果启用超时
  │   └── 启动 m_timeoutTimer（单次，间隔 taskTimeoutMs）
  │       └── 超时触发 → emit sigTaskTimeout(taskId) → DBBridge 收到后标记已超时
  │
  ├── 投递到 DBTaskManager::onPushTask(task)
  │   └── pushTask → 入队 → onTaskLoop → execTask
  │
  └── DBTaskManager::processTask()
      └── m_dbConn->execTask(task, result)
          └── [新增] 各 SqlUnit 执行完毕后，检查 totalDuration > taskTimeoutMs
              ├── 是 → rollback → result.errCode = TASK_EXPIRED → 不 commit
              └── 否 → 正常 commit 或继续下一条 SqlUnit
```

### 2.2 超时检测的两层设计

| 层级 | 位置 | 检测时机 | 效果 |
|------|------|---------|------|
| **应用层超时** | `DBService` 类 | `onExecSqlList` 后启动 `QTimer`，到期时 `sigExecFinished` 仍未触发 | 先返回超时结果给调用方，实际 SQL 完成后丢弃 |
| **连接层超时** | `QDBConnection::execTask()` | 每条 `SqlUnit` 执行完毕，在 commit 前检查 `duration > timeoutMs` | 超时则 rollback，返回 `TASK_EXPIRED` |

### 2.3 关于 SQL 长时间阻塞 QDBConnection 的行为

> 这是关键注意事项，直接回答**当数据库长时间无反应时的表现**。

在 `WorkerThread` 模式下，`DBTaskManager` 和 `QDBConnection` 运行在工作线程。`execTask()` 是同步阻塞调用，一旦进入 SQL 执行，工作线程的 Qt 事件循环被阻塞。

| 场景 | 表现 |
|------|------|
| **单条 SQL 长时间无返回**（如死锁、网络中断） | 工作线程完全阻塞，`QTimer`（超时定时器、重制定时器）**全部无法触发**。应用层超时 `QTimer` 在主线程可正常触发 |
| **多条 SqlUnit，中间某条卡住** | 前面的 SqlUnit 已执行完成并获取结果，当前 SqlUnit 卡住，后续 SqlUnit 等待 |
| **队列中其他任务被阻塞** | 当前 `DBTask` 在线程中阻塞，队列中后续的 `DBTask` 全部等待。相当于"一个慢 SQL 阻塞了整条队列" |
| **应用层超时触发后** | `sigTaskTimeout` 发出，调用方（DBBridge）标记已超时。工作线程的 SQL 继续执行直到返回，无法中途取消 |

**结论**：超时检测的粒度是**两条 SqlUnit 之间**和**全部 SqlUnit 完成后**。单条超长 SQL 无法中途打断（Qt 的 `QSqlQuery` 不支持跨驱动可靠取消），但可以通过应用层定时器先通知调用方。

---

## 三、具体改造项

### 3.1 DBConfig（数据库连接配置） — 新增超时字段

**文件**：`DBService_v1.2/include/DBConfig.h`（对外头文件）

```cpp
// 新增到 DBConfig 结构体
struct DBConfig
{
    // ... 原有字段不变 ...

    // ⭐ 新增：超时检测配置
    bool    enableTaskTimeout = false;   // 是否启用任务超时检测（默认不启用）
    int     taskTimeoutMs = 5000;        // 任务超时阈值（毫秒），默认 5 秒
};
```

**影响**：`DBConfig` 是对外暴露的头文件，新增字段有默认值，不破坏现有调用方。

**元对象注册**：无需额外注册，`DBConfig` 不跨线程传递。

### 3.2 DBErrCode（错误码枚举） — 新增 TASK_EXPIRED

**文件**：`DBService_v1.2/include/DBConfig.h`

```cpp
enum class DBErrCode
{
    SUCCESS        = 0,
    DB_NOT_OPEN    = 1,
    SQL_SYNTAX_ERR = 2,
    EXECUTE_FAILED = 3,
    TRANS_ERR      = 4,
    TASK_EXPIRED   = 5   // ⭐ 新增：任务执行超时，已回滚
};
```

**影响**：新增枚举值不破坏现有 `switch`（default 分支能处理）。跨线程信号参数中可能传递 `DBErrCode`，但由于它包含在 `DBTaskResult` 中（已注册 `Q_DECLARE_METATYPE(DBTaskResult)`），无需单独注册。

### 3.3 DBService 对外接口 — 新增超时信号

**文件**：`DBService_v1.2/include/DBService.h`

```cpp
class DBSERVICE_EXPORT DBService : public QObject
{
    Q_OBJECT
public:
    // ... 原有接口不变 ...

    // ⭐ 新增：超时配置
    void setTaskTimeoutEnabled(bool enabled);
    bool isTaskTimeoutEnabled() const;
    void setTaskTimeoutMs(int ms);
    int  taskTimeoutMs() const;

signals:
    // ... 原有信号不变 ...
    void sigExecFinished(const DBServiceResult& result);

    // ⭐ 新增：任务超时信号（应用层超时，SQL 可能仍在执行）
    void sigTaskTimedOut(const QString& taskId);
};
```

### 3.4 DBService 内部实现 — 添加超时定时器

**文件**：`DBService_v1.2/src/DBService/dbservice.cpp`

```cpp
// DBService 新增成员
QTimer* m_taskTimeoutTimer = nullptr;

// 在 start() 中初始化定时器
void DBService::start()
{
    if (m_config.dbConnConfig.enableTaskTimeout)
    {
        if (!m_taskTimeoutTimer)
        {
            m_taskTimeoutTimer = new QTimer(this);
            m_taskTimeoutTimer->setSingleShot(true);
            connect(m_taskTimeoutTimer, &QTimer::timeout, this, [this]() {
                // 超时触发
                if (m_currentTaskId.isEmpty()) return;
                emit sigTaskTimedOut(m_currentTaskId);
                // 注意：不清理 m_currentTaskId，等实际结果回来后再清理
            });
        }
    }

    if (!m_taskManager) return;
    // ... 原有 start 逻辑不变 ...
}

void DBService::onExecSqlList(const QVector<SqlTuple>& sqlList)
{
    // ... 原有逻辑不变：创建 DBTask ...

    // ⭐ 新增：启动超时定时器
    if (m_taskTimeoutTimer && m_config.dbConnConfig.enableTaskTimeout)
    {
        m_currentTaskId = task.taskId;
        m_taskTimeoutTimer->start(m_config.dbConnConfig.taskTimeoutMs);
    }

    // 投递任务
    // ...
}

void DBService::onTaskManagerComplete(const DBTaskResult& result)
{
    // ⭐ 新增：停止超时定时器
    if (m_taskTimeoutTimer && m_taskTimeoutTimer->isActive())
    {
        m_taskTimeoutTimer->stop();
    }
    m_currentTaskId.clear();

    // ... 原有 convertResult + emit 逻辑不变 ...
}
```

### 3.5 QDBConnection::execTask() — 连接层超时检测

**文件**：`DBService_v1.2/src/DbAccess/QDBConnection/qdbconnection.cpp`

```cpp
void QDBConnection::execTask(const DBTask& task, DBTaskResult& outResult)
{
    // ... 原有 ping + 事务开始逻辑不变 ...

    // ⭐ 新增：在执行过程中检查超时
    for (int i = 0; i < task.sqlList.size(); ++i)
    {
        const SqlUnit& unit = task.sqlList[i];
        QJsonArray resultArray;

        // ⚡ 新增：执行前检查是否已超时（主要用于队列积压场景）
        if (m_config.enableTaskTimeout)
        {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - task.requestTime > m_config.taskTimeoutMs)
            {
                // 已超时，回滚事务
                if (m_db.isOpen()) m_db.rollback();
                
                outResult.isSuccess = false;
                outResult.errCode = DBErrCode::TASK_EXPIRED;
                outResult.errMsg = QString("Task expired before execution (duration: %1ms, threshold: %2ms)")
                                   .arg(now - task.requestTime).arg(m_config.taskTimeoutMs);
                outResult.errUnitIndex = i;
                outResult.resultJson = QJsonDocument();
                return;
            }
        }

        if (!executeSqlUnit(unit, resultArray))
        {
            // ... 原有失败处理不变 ...
        }

        // ⚡ 新增：SqlUnit 执行完毕后，检查是否超时
        if (m_config.enableTaskTimeout && !unit.isModify)
        {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - task.requestTime > m_config.taskTimeoutMs)
            {
                // 超时，回滚已执行的所有操作
                if (m_db.isOpen()) m_db.rollback();
                
                outResult.isSuccess = false;
                outResult.errCode = DBErrCode::TASK_EXPIRED;
                outResult.errMsg = QString("Task expired during execution (duration: %1ms, threshold: %2ms)")
                                   .arg(now - task.requestTime).arg(m_config.taskTimeoutMs);
                outResult.errUnitIndex = i;
                outResult.resultJson = QJsonDocument();
                return;
            }
        }

        // 将执行结果组装到JSON
        if (!resultArray.isEmpty())
        {
            resultObj[unit.jsonKey] = resultArray;
        }
    }

    // ⚡ 新增：提交前检查是否超时（最后一道防线）
    if (m_config.enableTaskTimeout)
    {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - task.requestTime > m_config.taskTimeoutMs)
        {
            if (m_db.isOpen()) m_db.rollback();
            
            outResult.isSuccess = false;
            outResult.errCode = DBErrCode::TASK_EXPIRED;
            outResult.errMsg = QString("Task expired before commit (duration: %1ms, threshold: %2ms)")
                               .arg(now - task.requestTime).arg(m_config.taskTimeoutMs);
            outResult.errUnitIndex = -1;
            outResult.resultJson = QJsonDocument();
            return;
        }
    }

    // 提交事务（原有逻辑）
    if (!m_db.commit()) { /* 原有处理 */ }

    outResult.isSuccess = true;
    outResult.errCode = DBErrCode::SUCCESS;
    outResult.resultJson = QJsonDocument(resultObj);
}
```

### 3.6 DBService 返回结果结构体 — 扩展超时字段

**文件**：`DBService_v1.2/include/DBServiceStruct.h`

```cpp
struct DBServiceResult
{
    // ... 原有字段不变 ...
    bool isSuccess;
    QString serviceName;
    QString taskId;
    QVector<SqlTuple> sqlList;
    DBErrCode errCode;
    QString errMsg;
    QString errSql;
    int errTupleIndex;
    QMap<QString, QVariant> data;

    // ⭐ 新增：超时标识
    bool isTimeout = false;  // true = 应用层超时（sigTaskTimedOut 触发）
};
```

### 3.7 DBConfig.ini 配置文件 — 新增超时配置段

**文件**：`DBService_v1.2/docs/config/DBConfig.ini`

```ini
[Database]
; ... 原有配置不变 ...

; ============================================
; ⭐ 任务超时配置（v1.3 新增）
; ============================================
[Timeout]
; 是否启用任务超时检测（默认 false，不启用）
enableTaskTimeout=false
; 任务超时阈值（毫秒），默认 5000ms
taskTimeoutMs=5000
```

**默认值**：所有配置项在代码中都有默认值，即使缺少配置文件也能正常启动。`enableTaskTimeout` 默认 `false`，保持向后兼容。

---

## 四、元对象注册检查

DBService 中需要跨线程传递的类型及其注册状态：

| 类型 | 是否已注册 | 说明 |
|------|-----------|------|
| `DBTask` | 已注册（`dbtask.h`） | 跨线程 `Q_ARG(DBTask)` |
| `DBTaskResult` | 已注册（`dbtaskresult.h`） | 跨线程 `sigTaskComplete` |
| `DBServiceResult` | **新增注册** | 跨线程 `sigExecFinished`，需在 `DBService` 构造函数中加入 |
| `SqlTuple` | 已注册（`DBServiceStruct.h`） | 使用中 |
| `QVector<SqlTuple>` | 已注册（`DBServiceStruct.h`） | 使用中 |
| `QVector<QVariantMap>` | 已注册（`DBServiceStruct.h`） | 使用中 |

**需要新增的注册**：在 `DBService` 构造函数中添加：

```cpp
DBService::DBService(/* ... */)
{
    // 原有注册
    qRegisterMetaType<DBTask>("DBTask");
    qRegisterMetaType<DBTaskResult>("DBTaskResult");
    // ⭐ 新增注册
    qRegisterMetaType<DBServiceResult>("DBServiceResult");
}
```

---

## 五、启用方式

### 5.1 通过配置文件启用

```ini
[Timeout]
enableTaskTimeout=true
taskTimeoutMs=5000
```

使用 `loadSimpleConfig()` 时会自动加载，DBService 构造函数中根据 `DBConfig.enableTaskTimeout` 决定是否启动超时定时器。

### 5.2 代码中启用

```cpp
// 方式一：构造前修改 DBConfig
DBConfig dbConfig;
dbConfig.enableTaskTimeout = true;
dbConfig.taskTimeoutMs = 3000;  // 3 秒

DBService* db = new DBService(DBServiceType::Standard, dbConfig, this);
db->start();

// 方式二：运行时动态设置
db->setTaskTimeoutEnabled(true);
db->setTaskTimeoutMs(3000);

// 连接超时信号
connect(db, &DBService::sigTaskTimedOut, this, [](const QString& taskId) {
    qWarning() << "Task timed out:" << taskId;
});
```

### 5.3 默认不启用的处理

当 `enableTaskTimeout = false` 时：
- `m_taskTimeoutTimer` 不创建，不消耗定时器资源
- `QDBConnection::execTask()` 中的超时检查分支跳过
- `DBServiceResult::isTimeout` 始终为 `false`
- 性能零开销

---

## 六、影响范围总结

| 文件 | 修改类型 | 影响 |
|------|---------|------|
| `include/DBConfig.h` | 新增字段 | 有默认值，不破坏现有调用方 |
| `include/DBServiceStruct.h` | 新增字段 | `isTimeout` 默认 false，不破坏 |
| `include/DBService.h` | 新增信号 + setter | 新增，不破坏现有 API |
| `src/DBService/dbservice.cpp` | 新增定时器逻辑 | 内部实现，对外不可见 |
| `src/DbAccess/QDBConnection/qdbconnection.cpp` | 新增超时检查 | 条件编译，默认跳过分支 |
| `src/DbAccess/DbAccessStruct/dbconfig.h` | 新增枚举值 + 结构体字段 | `DBErrCode::TASK_EXPIRED` 有默认值 |

**对外接口兼容**：所有新增字段有默认值（`false` / `0` / `5000`），现有使用 DBService 的代码**无需修改**。
