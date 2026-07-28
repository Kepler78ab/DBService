# DBService 扩展原始 JSON 信号方案

## 背景

当前数据流存在 JSON → Qt 类型 → JSON 的无用往返转换：

```
DBTaskResult.resultJson  (QJsonDocument，原始 JSON)
    ↓ DBService::extractData()
DBServiceResult.data     (QVariantMap，含 QVector<QVariantMap>)
    ↓ DBBridgeServer::trySendDbResult()
WebSocket 响应           (QJsonObject)
```

`extractData` 将原始 JSON 拆解成 `QVector<QVariantMap>`，而后端又要再组装回 JSON，且 `QJsonValue::fromVariant` 不识别 `QVector<QVariantMap>` 类型导致数据丢失。

## 改造目标

在 DBService 新增一个信号，直传原始 `QJsonObject`，跳过中间的 Qt 类型转换。DBBridgeServer 直接连接新信号转发，零转换。

---

## 改动步骤

### 步骤 1：DBService DLL 源码 — dbservice.h

**文件：** `DBService_v1.2/src/DBService/dbservice.h`

在现有 `sigExecFinished` 信号之后追加新信号：

```cpp
signals:
    // 原有信号（保留，兼容其他使用者）
    void sigExecFinished(const DBServiceResult& result);

    // 新增信号：直传原始 JSON 结果，跳过 extractData 的 Qt 类型转换
    // taskId: 请求追踪 ID
    // resultJson: 原始查询结果 QJsonObject（键为 SqlBatchItem.tag，值为行数组或 affectedRows）
    // isSuccess: 执行是否成功
    // errCode:  错误码
    // errMsg:   错误描述
    // errSql:   出错 SQL
    void sigRawResult(const QString& taskId,
                      const QJsonObject& resultJson,
                      bool isSuccess,
                      int errCode,
                      const QString& errMsg,
                      const QString& errSql);
```

### 步骤 2：DBService DLL 源码 — dbservice.cpp

**文件：** `DBService_v1.2/src/DBService/dbservice.cpp`

在 `onTaskManagerComplete` 末尾，`emit sigExecFinished` 之后增加：

```cpp
void DBService::onTaskManagerComplete(const DBTaskResult& result)
{
    // ... 现有代码 ...
    emit sigExecFinished(serviceResult);

    // ---- 新增：发射原始 JSON 信号 ----
    QJsonObject rootObj = result.resultJson.object();
    emit sigRawResult(result.task.taskId,
                      rootObj,
                      result.isSuccess,
                      static_cast<int>(result.errCode),
                      result.errMsg,
                      result.errSql);
}
```

> **注意：** `result.resultJson` 已经是 `QJsonDocument`，直接调用 `.object()` 即可，无需任何转换。

### 步骤 3：重新编译 DBService DLL

- 用 Qt Creator 或命令行编译 `DBService_v1.2/DBService_v1.2.pro`
- 编译产物：
  - debug: `DBService_v1.2/bin/debug/DBService.dll`
  - release: `DBService_v1.2/bin/release/DBService.dll`

### 步骤 4：替换 DLL

将新编译的 DLL 拷贝到 server 引用目录：

| 源 | 目标 |
|---|---|
| `DBService_v1.2/bin/debug/DBService.dll` | `server/src/3rdparty/DBService/lib/debug/DBService.dll` |
| `DBService_v1.2/bin/release/DBService.dll` | `server/src/3rdparty/DBService/lib/release/DBService.dll` |
| `DBService_v1.2/bin/debug/DBService.dll` | `server/bin/debug/DBService.dll`（运行时目录） |

> 如果 DBService 的 `.pro` 中 `DESTDIR` 直接输出到 `server/` 的对应目录，可跳过拷贝步骤。

### 步骤 5：修改 DBBridgeServer

**文件：** `server/src/ipc/DBBridgeServer.h`

新增连接新信号的槽函数声明：

```cpp
private slots:
    void onDbServiceResult(const DBServiceResult& result);         // 保留现有
    void onRawDbResult(const QString& taskId,
                       const QJsonObject& resultJson,
                       bool isSuccess,
                       int errCode,
                       const QString& errMsg,
                       const QString& errSql);  // 新增
```

**文件：** `server/src/ipc/DBBridgeServer.cpp`

#### 5a. 在 `setDBService` 中连接新信号

```cpp
void DBBridgeServer::setDBService(DBService* service)
{
    m_dbService = service;
    if (m_dbService) {
        connect(m_dbService, &DBService::sigExecFinished,
                this, &DBBridgeServer::onDbServiceResult);
        // 新增：连接原始 JSON 信号
        connect(m_dbService, &DBService::sigRawResult,
                this, &DBBridgeServer::onRawDbResult);
    }
}
```

#### 5b. 实现 `onRawDbResult`

```cpp
void DBBridgeServer::onRawDbResult(const QString& taskId,
                                    const QJsonObject& resultJson,
                                    bool isSuccess,
                                    int errCode,
                                    const QString& errMsg,
                                    const QString& errSql)
{
    Q_UNUSED(errSql);
    // 遍历所有客户端，找到匹配的挂起请求
    for (auto* handler : m_clients) {
        handler->trySendRawResult(taskId, resultJson, isSuccess, errCode, errMsg);
    }
}
```

#### 5c. 在 ClientHandler 中新增 `trySendRawResult`

```cpp
void ClientHandler::trySendRawResult(const QString& taskId,
                                      const QJsonObject& resultJson,
                                      bool isSuccess,
                                      int errCode,
                                      const QString& errMsg)
{
    if (m_pendingIpcRequestId.isEmpty()) return;
    if (taskId != m_pendingIpcRequestId) return;

    QJsonObject responseObj;
    responseObj[IPCProtocol::KEY_REQUEST_ID]  = m_pendingIpcRequestId;
    responseObj[IPCProtocol::KEY_REQUEST_CMD] = QJsonValue();
    responseObj[IPCProtocol::KEY_IS_SUCCESS]  = isSuccess;
    responseObj[IPCProtocol::KEY_ERR_CODE]    = errCode;
    responseObj[IPCProtocol::KEY_ERR_MSG]     = errMsg;
    responseObj[IPCProtocol::KEY_DATA]        = resultJson;  // 直接使用原始 JSON，零转换

    QJsonDocument respDoc(responseObj);
    m_socket->sendTextMessage(QString::fromUtf8(respDoc.toJson(QJsonDocument::Compact)));

    qDebug() << "[DBBridgeServer] Sent raw response for:" << m_pendingIpcRequestId
             << "success:" << isSuccess;

    m_pendingIpcRequestId.clear();
    m_pendingSqlTuples.clear();
}
```

#### 5d. 添加必要的 include

```cpp
#include <QJsonObject>   // 如果还没有
```

## 完成后数据流

```
DBTaskResult.resultJson  (QJsonDocument)
    ↓ sigRawResult 信号（零转换）
DBBridgeServer::onRawDbResult
    ↓ trySendRawResult（直接赋值 QJsonObject）
WebSocket 响应           (QJsonObject)
```

不再经过 `extractData` 的 Qt 类型转换，数据完整性有保障，性能也更高。

## 注意事项

1. `sigExecFinished` 保留不删，其他已有使用者（如日志模块）继续用旧信号。
2. 如果确定 `DBBridgeServer` 是唯一消费者，也可以考虑只保留新信号，但建议先并存观察。
3. DBService DLL 依赖 Qt 5.12.x/5.15.2 编译，需确保编译环境与 server 项目一致，否则可能出现 ABI 不兼容。
