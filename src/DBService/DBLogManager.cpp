#include "DBLogManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSettings>
#include <QTextStream>

LogConfig LogConfig::loadFromIni(const QString& filePath)
{
    LogConfig cfg;
    QSettings settings(filePath, QSettings::IniFormat);

    settings.beginGroup("Log");
    cfg.logDir = settings.value("logDir", "service_log").toString();
    cfg.flushIntervalMs = settings.value("flushIntervalMs", 30000).toInt();
    settings.endGroup();

    qInfo() << "[DBLogManager] Loaded LogConfig:" << cfg.logDir << cfg.flushIntervalMs << "ms";
    return cfg;
}

DBLogManager::DBLogManager(QObject* parent)
    : QObject(parent)
{
}

DBLogManager::~DBLogManager()
{
    stop();
}

void DBLogManager::start(const QString& logDir,
                          int flushIntervalMs)
{
    if (m_workerThread) {
        qWarning() << "[DBLogManager] Already started, ignoring";
        return;
    }

    // 解析日志目录
    QDir dir(logDir);
    if (dir.isAbsolute()) {
        m_logDir = logDir;
    } else {
        m_logDir = QCoreApplication::applicationDirPath() + "/" + logDir;
    }
    QDir().mkpath(m_logDir);
    qInfo() << "[DBLogManager] Log directory:" << m_logDir;

    // 创建定时器（无父对象，将移至工作线程）
    m_flushTimer = new QTimer(nullptr);
    m_flushTimer->setInterval(flushIntervalMs);

    // 创建并启动工作线程
    m_workerThread = new QThread(this);

    // 将自身和定时器移至工作线程
//    moveToThread(m_workerThread);
    m_flushTimer->moveToThread(m_workerThread);

    // onRequestRecord 通过 QueuedConnection 在工作线程的元事件循环中执行入队
    connect(this, &DBLogManager::sigRequestRecord,
            this, &DBLogManager::onRequestRecord, Qt::QueuedConnection);

    // flushToFile 通过 DirectConnection 在工作线程执行
    connect(m_flushTimer, &QTimer::timeout,
            this, &DBLogManager::flushToFile, Qt::DirectConnection);

    // 线程启动后启动定时器
    connect(m_workerThread, &QThread::started,
            m_flushTimer, [this]() { m_flushTimer->start(); });

    // 线程退出时停止定时器
    connect(m_workerThread, &QThread::finished,
            m_flushTimer, &QTimer::stop);

    m_workerThread->start();

    qInfo() << "[DBLogManager] Started, flushInterval:" << flushIntervalMs << "ms";
}

void DBLogManager::stop()
{
    if (!m_workerThread) return;

    qInfo() << "[DBLogManager] Stopping...";

    // 先移回主线程，确保安全析构
    if (m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }

    // 最终刷盘
    flushToFile();

    // 清理定时器
    if (m_flushTimer) {
        delete m_flushTimer;
        m_flushTimer = nullptr;
    }

    delete m_workerThread;
    m_workerThread = nullptr;

    qInfo() << "[DBLogManager] Stopped";
}

void DBLogManager::onRequestRecord(const DBServiceResult& result)
{
    QMutexLocker locker(&m_mutex);
    m_pendingResults.append(result);
}

void DBLogManager::flushToFile()
{
    QVector<DBServiceResult> batch;
    {
        QMutexLocker locker(&m_mutex);
        if (m_pendingResults.isEmpty()) return;
        batch.swap(m_pendingResults);
    }

    // 按 serviceName 分组，写入各自的文件
    QMap<QString, QVector<DBServiceResult>> groups;
    for (const DBServiceResult& result : batch) {
        const QString serviceName = result.serviceName.isEmpty() ? "default" : result.serviceName;
        groups[serviceName].append(result);
    }

    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        const QString& svcName = it.key();
        const QVector<DBServiceResult>& results = it.value();

        const QString filePath = logFilePath(svcName);
        QDir().mkpath(QFileInfo(filePath).absolutePath());

        QFile file(filePath);
        if (!file.open(QIODevice::Append | QIODevice::Text)) {
            qWarning() << "[DBLogManager] Cannot open log file:" << filePath;
            continue;
        }

        QTextStream out(&file);
        int written = 0;
        for (const DBServiceResult& result : results) {
            const QString line = buildJsonLine(result);
            if (!line.isEmpty()) {
                out << line << "\n";
                ++written;
            }
        }
        file.close();

        if (written > 0) {
            qInfo() << "[DBLogManager] Flushed" << written << "records to" << filePath;
            emit sigLogWritten(filePath, written);
        }
    }
}

QString DBLogManager::logFilePath(const QString& serviceName) const
{
    const QString dateStr = QDateTime::currentDateTime().toString("yyyyMMdd");
    return m_logDir + "/" + serviceName + "_" + dateStr + ".txt";
}

QString DBLogManager::buildJsonLine(const DBServiceResult& result) const
{
    QJsonObject obj;
    obj["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    obj["logType"] = "db_result";
    obj["serviceName"] = result.serviceName;
    obj["isSuccess"] = result.isSuccess;
    obj["taskId"] = result.taskId;
    obj["errCode"] = static_cast<int>(result.errCode);
    obj["errMsg"] = result.errMsg;
    obj["errSql"] = result.errSql;
    obj["errTupleIndex"] = result.errTupleIndex;

    QJsonArray sqlArray;
    for (const SqlTuple& tuple : result.sqlList) {
        QJsonObject sqlObj;
        sqlObj["tag"] = tuple.tag;
        sqlObj["sql"] = tuple.sql;
        sqlObj["isModify"] = tuple.isModify;
        sqlArray.append(sqlObj);
    }
    obj["sqlList"] = sqlArray;

    // 直接从 rawJson 读取，避免 data 往返转换
    if (!result.rawJson.isEmpty()) {
        obj["data"] = result.rawJson.object();
    } else {
        obj["data"] = QJsonObject();
    }

    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
