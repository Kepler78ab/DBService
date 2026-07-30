#ifndef DBSERVICE_H
#define DBSERVICE_H

#include <QObject>
#include <QSettings>
#include <QPair>
#include <QThread>
#include <QMutex>
#include "dbservicestruct.h"
#include "../dbservice_global.h"

struct DBTaskManagerConfig;
class DBTaskManager;
struct DBTaskResult;

/**
 * @brief 任务管理器线程模式
 */
enum class TaskThreadModel
{
    MainThread,
    WorkerThread
};

class DBSERVICE_EXPORT DBService : public QObject
{
    Q_OBJECT
public:
    explicit DBService(TaskThreadModel model = TaskThreadModel::MainThread, QObject* parent = nullptr);
    explicit DBService(DBServiceType type, const DBConfig& dbConfig,
                       TaskThreadModel model = TaskThreadModel::MainThread, QObject* parent = nullptr);
    explicit DBService(const QPair<DBConfig, DBServiceType>& simpleConfig,
                       TaskThreadModel model = TaskThreadModel::MainThread, QObject* parent = nullptr);
    ~DBService();

    static QString currentVersion();

    TaskThreadModel threadModel() const { return m_threadModel; }
    bool isWorkerThread() const { return m_threadModel == TaskThreadModel::WorkerThread; }

    bool init(DBServiceType type, const DBConfig& dbConfig);
    bool init(const QPair<DBConfig, DBServiceType>& simpleConfig);
    void start();

    static QPair<DBConfig, DBServiceType> loadSimpleConfig(const QString& filePath);

    QString serviceName() const { return m_serviceName; }
    void setServiceName(const QString& name) { m_serviceName = name; }

    static QMap<QString, QVariant> extractData(const QJsonDocument& rawJson);

public slots:
    void onExecSqlList(const QVector<SqlTuple>& sqlList);
    void onExecSqlList(const QVector<SqlTuple>& sqlList, const QString& taskId);

signals:
    void sigExecFinished(const DBServiceResult& result);

private slots:
    void onTaskManagerComplete(const DBTaskResult& result);

private:
    DBTaskManagerConfig generateConfig(DBServiceType type, const DBConfig& dbConfig);
    bool init(const DBTaskManagerConfig& config);
    DBServiceResult convertResult(const DBTaskResult& taskResult, const QVector<SqlTuple>& originalTupleList);

    DBServiceType m_serviceType;
    TaskThreadModel m_threadModel;
    DBTaskManager* m_taskManager = nullptr;
    QThread* m_workerThread = nullptr;
    QMutex m_pendingMutex;
    QMap<QString, QVector<SqlTuple>> m_pendingTasks;
    QString m_serviceName;
};

#endif // DBSERVICE_H
