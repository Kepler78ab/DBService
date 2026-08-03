#include "qdbconnection.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>
#include <QDebug>
#include <QException>
#include <QElapsedTimer>

QDBConnection::QDBConnection(QObject *parent)
    : QObject(parent)
    , m_isConnected(false)
    , m_connectionName("")
    , m_retryTimer(new QTimer(this))
    , m_retryCount(0)
{
    // 配置重试定时器为单次触发模式
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &QDBConnection::onRetryTimer);
}

QDBConnection::~QDBConnection()
{
    closeConnection();
}

bool QDBConnection::init(const DBConfig& config)
{
    try {
        m_config = config;
        m_retryCount = 0;

        // 尝试连接一次，不阻塞
        doConnect();

        return m_isConnected;
    } catch (const QException& e) {
        qCritical() << "Exception in QDBConnection::init:" << e.what();
        return false;
    } catch (...) {
        qCritical() << "Unknown exception in QDBConnection::init";
        return false;
    }
}

void QDBConnection::doConnect()
{
    try {
        // 如果已连接，先关闭
        if (m_db.isOpen())
        {
            m_db.close();
        }

        // Qt 要求：removeDatabase() 之前必须先销毁 QSqlDatabase 对象
        // 重置 m_db 为无效状态，释放对旧连接名的引用
        m_db = QSqlDatabase();

        // 如果已有连接名，移除旧连接
        if (!m_connectionName.isEmpty())
        {
            QSqlDatabase::removeDatabase(m_connectionName);
        }

        // 每次重连使用新连接名，避免底层驱动句柄未完全释放时复用旧连接名。
        static qint64 connectionCounter = 0;
        m_connectionName = QString("DBConnection_%1_%2").arg((quintptr)this).arg(++connectionCounter);

        // 根据配置获取驱动名称
        QString driverName;
        switch (m_config.driverType)
        {
        case DBDriverType::MySQL:
            driverName = "QMYSQL";
            break;
        case DBDriverType::Oracle:
            driverName = "QOCI";
            break;
        case DBDriverType::ODBC:
            driverName = "QODBC";
            break;
        case DBDriverType::PostgreSQL:
            driverName = "QPSQL";
            break;
        case DBDriverType::SQLite:
            driverName = "QSQLITE";
            break;
        case DBDriverType::DB2:
            driverName = "QDB2";
            break;
        default:
            qWarning() << "Unsupported driver type, fallback to QMYSQL";
            driverName = "QMYSQL";
        }

        m_db = QSqlDatabase::addDatabase(driverName, m_connectionName);
        m_db.setHostName(m_config.host);
        m_db.setPort(m_config.port);
        m_db.setDatabaseName(m_config.dbName);
        m_db.setUserName(m_config.user);
        m_db.setPassword(m_config.password);

        // 设置连接超时
        // m_db.setConnectOptions("connect_timeout=5");

        if (m_db.open())
        {
            m_isConnected = true;
            m_retryCount = 0;
            // 停止重试定时器
            if (m_retryTimer->isActive())
            {
                m_retryTimer->stop();
            }
            qDebug() << "Database connected successfully";
            emit sigConnectionChanged(true);
        }
        else
        {
            m_isConnected = false;
            qWarning() << "Database connection failed:" << m_db.lastError().text();

            // 检查是否需要重试
            if (m_config.enableRetry)
            {
                int maxAttempts = m_config.limitRetryCount ? m_config.retryTimes : -1; // -1表示无限重试

                if (maxAttempts == -1 || m_retryCount < maxAttempts)
                {
                    m_retryCount++;
                    qDebug() << "Scheduling retry attempt" << m_retryCount << "after" << m_config.retryIntervalMs << "ms";
                    // 使用 QTimer 实现非阻塞延时重试
                    m_retryTimer->start(m_config.retryIntervalMs);
                }
                else
                {
                    qWarning() << "Max retry attempts reached, giving up";
                }
            }
        }
    } catch (const QException& e) {
        qCritical() << "Exception in QDBConnection::doConnect:" << e.what();
        m_isConnected = false;
        
        if (m_config.enableRetry) {
            int maxAttempts = m_config.limitRetryCount ? m_config.retryTimes : -1;
            if (maxAttempts == -1 || m_retryCount < maxAttempts) {
                m_retryCount++;
                m_retryTimer->start(m_config.retryIntervalMs);
            }
        }
    } catch (...) {
        qCritical() << "Unknown exception in QDBConnection::doConnect";
        m_isConnected = false;
        
        if (m_config.enableRetry) {
            int maxAttempts = m_config.limitRetryCount ? m_config.retryTimes : -1;
            if (maxAttempts == -1 || m_retryCount < maxAttempts) {
                m_retryCount++;
                m_retryTimer->start(m_config.retryIntervalMs);
            }
        }
    }
}

void QDBConnection::onRetryTimer()
{
    try {
        qDebug() << "Retry timer triggered, attempting to reconnect...";
        doConnect();
    } catch (const QException& e) {
        qCritical() << "Exception in QDBConnection::onRetryTimer:" << e.what();
    } catch (...) {
        qCritical() << "Unknown exception in QDBConnection::onRetryTimer";
    }
}

void QDBConnection::closeConnection()
{
    try {
        // 停止重试定时器
        if (m_retryTimer->isActive())
        {
            m_retryTimer->stop();
        }

        if (m_db.isOpen())
        {
            m_db.close();
        }

        if (!m_connectionName.isEmpty())
        {
            QSqlDatabase::removeDatabase(m_connectionName);
            m_connectionName.clear();
        }

        if (m_isConnected)
        {
            m_isConnected = false;
            emit sigConnectionChanged(false);
        }
    } catch (const QException& e) {
        qCritical() << "Exception in QDBConnection::closeConnection:" << e.what();
    } catch (...) {
        qCritical() << "Unknown exception in QDBConnection::closeConnection";
    }
}

bool QDBConnection::executeSqlUnit(const SqlUnit& unit, QJsonObject& resultObject)
{
    try {
        QSqlQuery query(m_db);

        // 设置只向前查询模式，优化查询性能
        query.setForwardOnly(true);

        if (!query.exec(unit.sql))
        {
            qWarning() << "SQL execution failed:" << query.lastError().text();
            query.finish();
            return false;
        }

        if (!unit.isModify)
        {
            // 非修改型语句：获取查询结果
            QSqlRecord record = query.record();
            // 列名在循环外预提取一次，避免每行每列重复构造/查找（大数据量下显著省 CPU）
            QStringList fieldNames;
            for (int i = 0; i < record.count(); ++i)
            {
                fieldNames << record.fieldName(i);
            }

            // 紧凑格式：每行为纯值 QJsonArray（无键名、无哈希节点）
            QJsonArray rows;
            while (query.next())
            {
                QJsonArray row;
                for (int i = 0; i < record.count(); ++i)
                {
                    QVariant value = query.value(i);

                    if (value.isNull())
                    {
                        row.append(QJsonValue::Null);
                    }
                    else if (value.type() == QVariant::Int)
                    {
                        row.append(value.toInt());
                    }
                    else if (value.type() == QVariant::LongLong)
                    {
                        row.append(value.toLongLong());
                    }
                    else if (value.type() == QVariant::Double)
                    {
                        row.append(value.toDouble());
                    }
                    else if (value.type() == QVariant::Bool)
                    {
                        row.append(value.toBool());
                    }
                    else
                    {
                        row.append(value.toString());
                    }
                }
                rows.append(row);
            }

            resultObject["type"] = QStringLiteral("query");
            resultObject["columns"] = QJsonArray::fromStringList(fieldNames);
            resultObject["rows"] = rows;
        }
        else
        {
            // 修改型语句：返回影响行数和最后插入ID
            resultObject["type"] = QStringLiteral("write");
            resultObject["affectedRows"] = query.numRowsAffected();

            // 获取最后插入的ID（适用于INSERT语句）
            QVariant lastInsertId = query.lastInsertId();
            if (lastInsertId.isValid())
            {
                if (lastInsertId.type() == QVariant::Int)
                {
                    resultObject["lastInsertId"] = lastInsertId.toInt();
                }
                else if (lastInsertId.type() == QVariant::LongLong)
                {
                    resultObject["lastInsertId"] = lastInsertId.toLongLong();
                }
                else
                {
                    resultObject["lastInsertId"] = lastInsertId.toString();
                }
            }
        }

        query.finish();
        return true;
    } catch (const QException& e) {
        qCritical() << "Exception in QDBConnection::executeSqlUnit:" << e.what();
        return false;
    } catch (...) {
        qCritical() << "Unknown exception in QDBConnection::executeSqlUnit";
        return false;
    }
}

void QDBConnection::execTask(const DBTask& task, DBTaskResult& outResult)
{
    try {
        QElapsedTimer taskTimer;
        taskTimer.start();

        outResult.reset();
        outResult.task = task;

        qInfo() << "[QDBConnection] 执行开始 taskId=" << task.taskId
                << "unitCount=" << task.sqlList.size();

        // 健康检查：使用 SELECT 1 真实检测连接是否存活
        // isOpen() 在 TCP 断开后仍返回 true，不能作为判断依据
        if (m_isConnected)
        {
            bool alive = false;
            {
                QSqlQuery ping(m_db);
                if (ping.exec("SELECT 1")) {
                    alive = true;
                }
                ping.finish();
            } // ping 在此析构，释放 OCI statement handle

            if (!alive)
            {
                qWarning() << "Connection lost (ping failed), attempting reconnect...";
                doConnect();
            }
        }

        // 检查最终连接状态
        if (!m_isConnected || !m_db.isOpen())
        {
            outResult.isSuccess = false;
            outResult.errCode = DBErrCode::DB_NOT_OPEN;
            outResult.errMsg = "Database connection is not available";
            outResult.errUnitIndex = -1;
            return;
        }

        // 开始事务
        if (!m_db.transaction())
        {
            outResult.isSuccess = false;
            outResult.errCode = DBErrCode::TRANS_ERR;
            outResult.errMsg = "Failed to start transaction: " + m_db.lastError().text();
            outResult.errUnitIndex = -1;

            // 事务失败大概率是连接断开，触发后台重连
            if (!m_db.isOpen()) {
                m_isConnected = false;
                qWarning() << "Transaction failed, connection may be lost, triggering reconnect...";
                doConnect();
            }
            return;
        }

        QJsonObject resultObj;
        qint64 totalRows = 0;

        // 按顺序串行执行所有SQL单元
        for (int i = 0; i < task.sqlList.size(); ++i)
        {
            const SqlUnit& unit = task.sqlList[i];
            QJsonObject resultObject;

            if (!executeSqlUnit(unit, resultObject))
            {
                // SQL执行失败，立即回滚事务
                if (m_db.isOpen())
                {
                    m_db.rollback();
                }

                outResult.isSuccess = false;
                outResult.errCode = DBErrCode::EXECUTE_FAILED;
                outResult.errMsg = m_db.lastError().text();
                outResult.errSql = unit.sql;
                outResult.errUnitIndex = i;
                // 强制清空结果
                outResult.resultJson = QJsonDocument();
                return;
            }

            // 紧凑格式：总是放入（空结果保留列头，写操作保留 affectedRows）
            resultObj[unit.jsonKey] = resultObject;
            // 行数统计（日志用）：查询计 rows 长度，写计 affectedRows
            if (resultObject.value(QStringLiteral("type")).toString() == QStringLiteral("query"))
            {
                totalRows += resultObject.value(QStringLiteral("rows")).toArray().size();
            }
            else
            {
                totalRows += static_cast<qint64>(resultObject.value(QStringLiteral("affectedRows")).toDouble());
            }
        }

        // 全部SQL执行成功，提交事务
        if (!m_db.commit())
        {
            // 提交失败，尝试回滚
            if (m_db.isOpen())
            {
                m_db.rollback();
            }

            outResult.isSuccess = false;
            outResult.errCode = DBErrCode::TRANS_ERR;
            outResult.errMsg = "Failed to commit transaction: " + m_db.lastError().text();
            outResult.errUnitIndex = -1;
            outResult.resultJson = QJsonDocument();
            return;
        }

        // 执行成功
        outResult.isSuccess = true;
        outResult.errCode = DBErrCode::SUCCESS;
        outResult.errUnitIndex = -1;
        // 统一构造 QJsonDocument（resultObj 的键即各 SqlUnit 的 jsonKey/tag）
        outResult.resultJson = QJsonDocument(resultObj);

        qInfo() << "[QDBConnection] 任务完成 taskId=" << task.taskId
                << "isSuccess=" << outResult.isSuccess
                << "rows=" << totalRows
                << "elapsed=" << taskTimer.elapsed() << "ms";
    } catch (const QException& e) {
        qCritical() << "Exception in QDBConnection::execTask:" << e.what();
        
        // 回滚事务
        try {
            if (m_db.isOpen()) {
                m_db.rollback();
            }
        } catch (...) {}
        
        outResult.isSuccess = false;
        outResult.errCode = DBErrCode::EXECUTE_FAILED;
        outResult.errMsg = QString("Exception occurred: ") + e.what();
        outResult.errUnitIndex = -1;
        outResult.resultJson = QJsonDocument();
    } catch (...) {
        qCritical() << "Unknown exception in QDBConnection::execTask";
        
        // 回滚事务
        try {
            if (m_db.isOpen()) {
                m_db.rollback();
            }
        } catch (...) {}
        
        outResult.isSuccess = false;
        outResult.errCode = DBErrCode::EXECUTE_FAILED;
        outResult.errMsg = "Unknown exception occurred";
        outResult.errUnitIndex = -1;
        outResult.resultJson = QJsonDocument();
    }
}
