#include "dbservice.h"
#include "../DbAccess/DBTaskManager/dbtaskmanager.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>
#include <QVector>

// 前置声明：统计结果JSON中的总行数（所有 jsonKey 数组长度之和），用于日志打点
static qint64 countResultRows(const QJsonDocument& doc);

DBService::DBService(TaskThreadModel model, QObject* parent)
    : QObject(parent)
    , m_serviceType(DBServiceType::Standard)
    , m_threadModel(model)
    , m_taskManager(nullptr)
{
    qRegisterMetaType<DBTask>("DBTask");
    qRegisterMetaType<DBTaskResult>("DBTaskResult");
    qRegisterMetaType<DBServiceResult>("DBServiceResult");
}

DBService::DBService(DBServiceType type, const DBConfig& dbConfig,
                     TaskThreadModel model, QObject* parent)
    : QObject(parent)
    , m_serviceType(type)
    , m_threadModel(model)
    , m_taskManager(nullptr)
{
    qRegisterMetaType<DBTask>("DBTask");
    qRegisterMetaType<DBTaskResult>("DBTaskResult");
    qRegisterMetaType<DBServiceResult>("DBServiceResult");
    init(type, dbConfig);
}

DBService::DBService(const QPair<DBConfig, DBServiceType>& simpleConfig,
                     TaskThreadModel model, QObject* parent)
    : QObject(parent)
    , m_serviceType(simpleConfig.second)
    , m_threadModel(model)
    , m_taskManager(nullptr)
{
    qRegisterMetaType<DBTask>("DBTask");
    qRegisterMetaType<DBTaskResult>("DBTaskResult");
    qRegisterMetaType<DBServiceResult>("DBServiceResult");
    init(simpleConfig);
}

DBService::~DBService()
{
    // 1. 先停止工作线程
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }

    // 2. 再清理 TaskManager
    if (m_taskManager) {
        m_taskManager->deleteLater();
        m_taskManager = nullptr;
    }
}

QString DBService::currentVersion()
{
    return QStringLiteral("v1.5.1");
}

bool DBService::init(DBServiceType type, const DBConfig& dbConfig)
{
    m_serviceType = type;
    DBTaskManagerConfig config = generateConfig(type, dbConfig);
    return init(config);
}

bool DBService::init(const DBTaskManagerConfig& config)
{
    // 清理旧的 TaskManager 和线程
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }
    if (m_taskManager) {
        m_taskManager->deleteLater();
        m_taskManager = nullptr;
    }

    // WorkerThread 模式：TaskManager 不能有 parent（否则 moveToThread 静默失败）
    // MainThread 模式：TaskManager 使用 this 作为 parent，由 Qt 管理生命周期
    m_taskManager = (m_threadModel == TaskThreadModel::WorkerThread)
        ? new DBTaskManager(nullptr)
        : new DBTaskManager(this);

    connect(m_taskManager, &DBTaskManager::sigTaskComplete,
            this, &DBService::onTaskManagerComplete);

    if (!m_taskManager->init(config)) {
        qCritical() << "DBService: TaskManager init failed";
    }

    // WorkerThread 模式：将 TaskManager 移入子线程
    if (m_threadModel == TaskThreadModel::WorkerThread) {
        m_workerThread = new QThread(this);
        m_workerThread->setObjectName("DBService_Worker");
        m_taskManager->moveToThread(m_workerThread);
        connect(m_workerThread, &QThread::finished,
                m_taskManager, &QObject::deleteLater);
        m_workerThread->start(QThread::LowPriority);   // 低优先级：大查询解析不抢占主线程/其他进程 CPU
    }

    return true;
}

bool DBService::init(const QPair<DBConfig, DBServiceType>& simpleConfig)
{
    return init(simpleConfig.second, simpleConfig.first);
}

void DBService::start()
{
    if (!m_taskManager) return;

    if (m_threadModel == TaskThreadModel::WorkerThread) {
        QMetaObject::invokeMethod(m_taskManager, "start", Qt::QueuedConnection);
    } else {
        m_taskManager->start();
    }
}

int DBService::pendingCount() const
{
    QMutexLocker locker(&m_pendingMutex);
    return m_pendingTasks.size();
}

DBTaskManagerConfig DBService::generateConfig(DBServiceType type, const DBConfig& dbConfig)
{
    DBTaskManagerConfig config;
    config.dbConnConfig = dbConfig;

    switch (type)
    {
    case DBServiceType::Reliable:
        config.retryMode = TaskRetryMode::ForeverRetry;
        config.retryIntervalMs = 3000;
        config.enableBatchEnqueue = false;
        config.queueMaxSize = 100;
        config.queueFullPolicy = QueueFullPolicy::BLOCK;
        break;

    case DBServiceType::HighPerf:
        config.retryMode = TaskRetryMode::LimitedRetry;
        config.maxRetryCount = 3;
        config.retryIntervalMs = 1000;
        config.enableBatchEnqueue = true;
        config.batchIntervalMs = 100;
        config.batchMaxSize = 50;
        config.queueMaxSize = 500;
        config.queueFullPolicy = QueueFullPolicy::REJECT;
        break;

    case DBServiceType::Standard:
        config.retryMode = TaskRetryMode::LimitedRetry;
        config.maxRetryCount = 3;
        config.retryIntervalMs = 3000;
        config.enableBatchEnqueue = true;
        config.batchIntervalMs = 1000;
        config.batchMaxSize = 10;
        config.queueMaxSize = 100;
        config.queueFullPolicy = QueueFullPolicy::REJECT;
        break;

    case DBServiceType::Unreliable:
        config.retryMode = TaskRetryMode::NoRetry;
        config.maxRetryCount = 0;
        config.retryIntervalMs = 0;
        config.enableBatchEnqueue = false;
        config.queueMaxSize = 100;
        config.queueFullPolicy = QueueFullPolicy::REJECT;
        break;
    }

    return config;
}

void DBService::onExecSqlList(const QVector<SqlTuple>& sqlList)
{
    if (!m_taskManager || sqlList.isEmpty()) {
        qWarning() << "DBService: TaskManager not initialized or empty sqlList";
        return;
    }

    DBTask task = DBTask::spawnRandomQUuidTask();

    qint64 totalLen = 0;
    for (const SqlTuple& tuple : sqlList) {
        SqlUnit unit = SqlUnit::createSqlUnit(tuple.tag, tuple.sql, tuple.isModify);
        task.append(unit);
        totalLen += tuple.sql.length();
    }
    qInfo() << "[DBService] 收到请求 taskId=" << task.taskId
            << "sqlCount=" << sqlList.size() << "totalLen=" << totalLen;

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

void DBService::onExecSqlList(const QVector<SqlTuple>& sqlList, const QString& taskId)
{
    if (!m_taskManager || sqlList.isEmpty()) {
        qWarning() << "DBService: TaskManager not initialized or empty sqlList";
        return;
    }

    DBTask task;
    task.taskId = taskId;
    task.requestTime = QDateTime::currentMSecsSinceEpoch();

    qint64 totalLen = 0;
    for (const SqlTuple& tuple : sqlList) {
        SqlUnit unit = SqlUnit::createSqlUnit(tuple.tag, tuple.sql, tuple.isModify);
        task.append(unit);
        totalLen += tuple.sql.length();
    }
    qInfo() << "[DBService] 收到请求 taskId=" << task.taskId
            << "sqlCount=" << sqlList.size() << "totalLen=" << totalLen;

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

void DBService::onTaskManagerComplete(const DBTaskResult& result)
{
    // 线程安全地查找并移除对应的原始SqlTuple列表
    QVector<SqlTuple> originalTupleList;
    {
        QMutexLocker locker(&m_pendingMutex);
        auto it = m_pendingTasks.find(result.task.taskId);
        if (it != m_pendingTasks.end()) {
            originalTupleList = std::move(it.value());
            m_pendingTasks.erase(it);
        }
    }

    // 转换结果（只传 rawJson，不调用 extractData）
    DBServiceResult serviceResult = convertResult(result, originalTupleList);

    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - result.task.requestTime;
    qInfo() << "[DBService] 回调 taskId=" << result.task.taskId
            << "isSuccess=" << serviceResult.isSuccess
            << "rows=" << countResultRows(serviceResult.rawJson)
            << "elapsed=" << elapsed << "ms";

    // 唯一的信号发射
    emit sigExecFinished(serviceResult);
}

DBServiceResult DBService::convertResult(const DBTaskResult& taskResult, const QVector<SqlTuple>& originalTupleList)
{
    DBServiceResult result;
    result.serviceName   = m_serviceName;
    result.isSuccess     = taskResult.isSuccess;
    result.taskId        = taskResult.task.taskId;
    result.sqlList       = originalTupleList;
    result.errCode       = taskResult.errCode;
    result.errMsg        = taskResult.errMsg;
    result.errSql        = taskResult.errSql;
    result.errTupleIndex = taskResult.errUnitIndex;

    // 直接传递原始 JSON，零转换
    // data 字段保持为空，调用方可按需调用 DBService::extractData(rawJson)
    result.rawJson = taskResult.resultJson;

    return result;
}

// 统计结果JSON中的总行数（紧凑格式：所有 type=query 的 rows 长度之和），用于日志打点
static qint64 countResultRows(const QJsonDocument& doc)
{
    if (doc.isEmpty() || !doc.isObject()) {
        return 0;
    }
    qint64 rows = 0;
    const QJsonObject rootObj = doc.object();
    for (QJsonObject::const_iterator it = rootObj.begin(); it != rootObj.end(); ++it) {
        const QJsonObject meta = it.value().toObject();
        if (meta.value(QStringLiteral("type")).toString() == QStringLiteral("query")) {
            rows += meta.value(QStringLiteral("rows")).toArray().size();
        }
    }
    return rows;
}

QMap<QString, QVariant> DBService::extractData(const QJsonDocument& rawJson)
{
    QMap<QString, QVariant> data;

    if (rawJson.isEmpty()) {
        return data;
    }

    const QJsonObject rootObj = rawJson.object();
    const QStringList keys = rootObj.keys();

    for (const QString& key : keys) {
        const QJsonObject meta = rootObj.value(key).toObject();
        const QString type = meta.value(QStringLiteral("type")).toString();

        if (type == QStringLiteral("query")) {
            // 查询结果：{ "columns": [...], "rows": [[...]] } 紧凑结构
            QVariantMap vm;
            QStringList columns;
            const QJsonArray colsArr = meta.value(QStringLiteral("columns")).toArray();
            for (const QJsonValue& c : colsArr) {
                columns << c.toString();
            }
            vm["columns"] = columns;
            vm["rows"] = meta.value(QStringLiteral("rows")).toArray();
            data[key] = vm;
        }
        else if (type == QStringLiteral("write")) {
            // 写结果：{ "affectedRows": n[, "lastInsertId": x] }
            QVariantMap vm;
            vm["affectedRows"] = static_cast<qint64>(meta.value(QStringLiteral("affectedRows")).toDouble());
            if (meta.contains(QStringLiteral("lastInsertId"))) {
                vm["lastInsertId"] = static_cast<qint64>(meta.value(QStringLiteral("lastInsertId")).toDouble());
            }
            data[key] = vm;
        }
    }

    return data;
}

bool DBService::extractColumnsAndRows(const QJsonDocument& rawJson, const QString& tag,
                                      QStringList* columns, QJsonArray* rows)
{
    if (rawJson.isEmpty() || !rawJson.isObject()) {
        return false;
    }
    const QJsonObject rootObj = rawJson.object();
    if (!rootObj.contains(tag)) {
        return false;
    }
    const QJsonObject meta = rootObj.value(tag).toObject();
    if (meta.value(QStringLiteral("type")).toString() != QStringLiteral("query")) {
        return false;
    }
    if (columns) {
        columns->clear();
        const QJsonArray colsArr = meta.value(QStringLiteral("columns")).toArray();
        for (const QJsonValue& c : colsArr) {
            columns->append(c.toString());
        }
    }
    if (rows) {
        *rows = meta.value(QStringLiteral("rows")).toArray();
    }
    return true;
}

bool DBService::extractWriteResult(const QJsonDocument& rawJson, const QString& tag,
                                   qint64* affectedRows, qint64* lastInsertId)
{
    if (rawJson.isEmpty() || !rawJson.isObject()) {
        return false;
    }
    const QJsonObject rootObj = rawJson.object();
    if (!rootObj.contains(tag)) {
        return false;
    }
    const QJsonObject meta = rootObj.value(tag).toObject();
    if (meta.value(QStringLiteral("type")).toString() != QStringLiteral("write")) {
        return false;
    }
    if (affectedRows) {
        *affectedRows = static_cast<qint64>(meta.value(QStringLiteral("affectedRows")).toDouble());
    }
    if (lastInsertId) {
        *lastInsertId = static_cast<qint64>(meta.value(QStringLiteral("lastInsertId")).toDouble(-1.0));
    }
    return true;
}

QPair<DBConfig, DBServiceType> DBService::loadSimpleConfig(const QString& filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);

    DBConfig dbConfig;
    settings.beginGroup("Database");

    QString driverStr = settings.value("driverType", "MySQL").toString();
    if (driverStr == "MySQL") dbConfig.driverType = DBDriverType::MySQL;
    else if (driverStr == "Oracle") dbConfig.driverType = DBDriverType::Oracle;
    else if (driverStr == "ODBC") dbConfig.driverType = DBDriverType::ODBC;
    else if (driverStr == "PostgreSQL") dbConfig.driverType = DBDriverType::PostgreSQL;
    else if (driverStr == "SQLite") dbConfig.driverType = DBDriverType::SQLite;
    else if (driverStr == "DB2") dbConfig.driverType = DBDriverType::DB2;
    else dbConfig.driverType = DBDriverType::MySQL;

    dbConfig.host = settings.value("host", "127.0.0.1").toString();
    dbConfig.port = settings.value("port", 3306).toUInt();
    dbConfig.dbName = settings.value("dbName", "").toString();
    dbConfig.user = settings.value("user", "root").toString();
    dbConfig.password = settings.value("password", "").toString();
    dbConfig.enableRetry = settings.value("enableRetry", true).toBool();
    dbConfig.retryTimes = settings.value("retryTimes", 3).toInt();
    dbConfig.retryIntervalMs = settings.value("retryIntervalMs", 2000).toInt();
    dbConfig.limitRetryCount = settings.value("limitRetryCount", true).toBool();
    settings.endGroup();

    DBServiceType serviceType = DBServiceType::Reliable;
    settings.beginGroup("Service");
    QString typeStr = settings.value("serviceType", "Reliable").toString();
    if (typeStr == "Reliable") serviceType = DBServiceType::Reliable;
    else if (typeStr == "HighPerf") serviceType = DBServiceType::HighPerf;
    else if (typeStr == "Standard") serviceType = DBServiceType::Standard;
    else if (typeStr == "Unreliable") serviceType = DBServiceType::Unreliable;
    settings.endGroup();

    return QPair<DBConfig, DBServiceType>(dbConfig, serviceType);
}
