#ifndef DBSERVICE_H
#define DBSERVICE_H

#include <QObject>
#include <QSettings>
#include <QPair>
#include <QThread>
#include <QMutex>
#include "DBServiceStruct.h"
#include "dbservice_global.h"

struct DBTaskManagerConfig;
class DBTaskManager;
struct DBTaskResult;

/**
 * @brief 任务管理器线程模式
 * 
 * MainThread   — 旧行为：TaskManager 在主线程，SQL 执行会阻塞 UI
 * WorkerThread — 新行为：TaskManager 移入子线程，不阻塞 UI
 */
enum class TaskThreadModel
{
    MainThread,
    WorkerThread
};

/**
 * @brief 数据库服务上层封装类
 * @note 提供极简API，屏蔽底层TaskManager细节
 *
 * 支持四种服务类型：
 * - Reliable：ForeverRetry，保证SQL正确时100%执行
 * - HighPerf：批量入队+短间隔，适合高频场景
 * - Standard：有限重试，折中策略
 * - Unreliable：NoRetry，只执行一次
 *
 * @note 当前版本：v1.4.0
 */
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

    /**
     * @brief 获取当前线程模式
     */
    TaskThreadModel threadModel() const { return m_threadModel; }

    /**
     * @brief 检查是否运行在子线程模式
     */
    bool isWorkerThread() const { return m_threadModel == TaskThreadModel::WorkerThread; }

    bool init(DBServiceType type, const DBConfig& dbConfig);
    bool init(const QPair<DBConfig, DBServiceType>& simpleConfig);
    void start();

    static QPair<DBConfig, DBServiceType> loadSimpleConfig(const QString& filePath);

    QString serviceName() const { return m_serviceName; }
    void setServiceName(const QString& name) { m_serviceName = name; }

    /**
     * @brief 将原始JSON结果转换为结构化数据
     * @param rawJson 原始查询结果JSON
     * @return 结构化数据，键为SqlUnit.tag，值为行数组(QVector<QVariantMap>)或修改结果(QVariantMap)
     * 
     * @note 默认 sigExecFinished 返回的 DBServiceResult.data 为空。
     *       需要结构化数据时（如 UI 绑定），可调用此函数手动转换。
     * @code
     * connect(db, &DBService::sigExecFinished, this, [](const DBServiceResult& res) {
     *     auto data = DBService::extractData(res.rawJson);
     *     // data["users"] → QVector<QVariantMap>
     * });
     * @endcode
     */
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
