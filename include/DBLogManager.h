#ifndef DBLOGMANAGER_H
#define DBLOGMANAGER_H

#include <QObject>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <QString>
#include "DBServiceStruct.h"
#include "dbservice_global.h"

/**
 * @brief 日志配置结构体
 */
struct LogConfig
{
    QString logDir = "service_log";
    int flushIntervalMs = 30000;

    static LogConfig loadFromIni(const QString& filePath);
};

class DBSERVICE_EXPORT DBLogManager : public QObject
{
    Q_OBJECT
public:
    explicit DBLogManager(QObject* parent = nullptr);
    ~DBLogManager();

    void start(const QString& logDir = "service_log",
               int flushIntervalMs = 30000);

    void stop();

signals:
    void sigRequestRecord(const DBServiceResult& result);
    void sigLogWritten(const QString& filePath, int count);

private slots:
    void onRequestRecord(const DBServiceResult& result);
    void flushToFile();

private:
    QString logFilePath(const QString& serviceName) const;
    QString buildJsonLine(const DBServiceResult& result) const;

    QThread* m_workerThread = nullptr;
    QTimer* m_flushTimer = nullptr;
    QString m_logDir;
    QVector<DBServiceResult> m_pendingResults;
    mutable QMutex m_mutex;
};

#endif // DBLOGMANAGER_H
