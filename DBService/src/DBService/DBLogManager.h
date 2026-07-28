#ifndef DBLOGMANAGER_H
#define DBLOGMANAGER_H

#include <QObject>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <QString>
#include "dbservicestruct.h"
#include "dbservice_global.h"

/**
 * @brief 日志配置结构体
 * @note 由 loadLogConfig() 从 LogConfig.ini 读取
 */
struct LogConfig
{
    QString logDir = "service_log";          ///< 日志目录
    int flushIntervalMs = 30000;             ///< 刷盘间隔（毫秒）

    /**
     * @brief 从 INI 文件加载日志配置
     * @param filePath LogConfig.ini 路径
     * @return LogConfig 配置结构体
     */
    static LogConfig loadFromIni(const QString& filePath);
};

class DBSERVICE_EXPORT DBLogManager : public QObject
{
    Q_OBJECT
public:
    explicit DBLogManager(QObject* parent = nullptr);
    ~DBLogManager();

    /**
     * @brief 启动：创建工作线程和定时器
     * @param logDir 日志目录（相对或绝对，默认 service_log）
     * @param flushIntervalMs 刷盘间隔，默认 30000ms
     */
    void start(const QString& logDir = "service_log",
               int flushIntervalMs = 30000);

    /// 停止：刷盘 + 退出工作线程
    void stop();

signals:
    /// 异步记录入口 — 外部通过此信号提交结果（QueuedConnection 不阻塞主线程）
    void sigRequestRecord(const DBServiceResult& result);

    void sigLogWritten(const QString& filePath, int count);

private slots:
    /// 接收 sigRequestRecord，在工作线程的元事件循环中执行入队
    void onRequestRecord(const DBServiceResult& result);

    /// 在工作线程上执行的 flush
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
