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

    /**
     * @brief 当前排队中的任务数（已提交尚未回调的任务数）
     * @note 线程安全；供 DBServicePool 负载均衡选实例使用
     */
    int pendingCount() const;

    /**
     * @brief 将紧凑格式原始JSON转换为结构化数据
     * @param rawJson 任务的原始查询结果JSON（v1.5.0 紧凑格式）
     * @return 结构化数据，键为 SqlUnit.tag：
     *         - 查询 tag → QVariantMap{ "columns": QStringList, "rows": QJsonArray }
     *         - 写   tag → QVariantMap{ "affectedRows": qint64[, "lastInsertId": qint64] }
     */
    static QMap<QString, QVariant> extractData(const QJsonDocument& rawJson);

    /**
     * @brief 零转换提取查询结果（按 tag 定位紧凑格式中的 type=query 节点）
     * @param rawJson  任务的原始查询结果JSON
     * @param tag      SqlUnit 的 jsonKey/tag
     * @param columns  输出列名（可传 nullptr 跳过）
     * @param rows     输出行数组（可传 nullptr 跳过；赋值共享，不深拷贝）
     * @return tag 存在且为 query 类型时返回 true
     */
    static bool extractColumnsAndRows(const QJsonDocument& rawJson, const QString& tag,
                                      QStringList* columns, QJsonArray* rows);

    /**
     * @brief 提取写操作结果（按 tag 定位紧凑格式中的 type=write 节点）
     * @param rawJson       任务的原始查询结果JSON
     * @param tag           SqlUnit 的 jsonKey/tag
     * @param affectedRows  输出影响行数（可传 nullptr 跳过）
     * @param lastInsertId  输出最后插入ID（无该字段时为 -1；可传 nullptr 跳过）
     * @return tag 存在且为 write 类型时返回 true
     */
    static bool extractWriteResult(const QJsonDocument& rawJson, const QString& tag,
                                   qint64* affectedRows, qint64* lastInsertId);

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
    mutable QMutex m_pendingMutex;
    QMap<QString, QVector<SqlTuple>> m_pendingTasks;
    QString m_serviceName;
};

#endif // DBSERVICE_H
