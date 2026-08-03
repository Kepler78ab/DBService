#ifndef DBSERVICEPOOL_H
#define DBSERVICEPOOL_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include "DBServiceStruct.h"
#include "dbservice_global.h"

class DBService;

/**
 * @brief DBService 连接池（DBService 的拓展用法，无状态调度壳）
 *
 * 持有 poolSize 个 DBService 实例（WorkerThread 模式），负责：
 * - 任务分发：每次 execSql 自动选择当前排队最少的实例（最少任务数负载均衡）
 * - 信号聚合：统一转发 sigExecFinished，调用方只需连接一个信号
 * - 配置克隆：baseConfig 拷贝 poolSize 份，实例命名 pool_0 / pool_1 / ...
 *
 * 与直接使用 DBService 的关系：
 * - DBServicePool 是 DBService 的拓展用法，二者可自由选择；
 * - poolSize=1 时行为等同单 DBService（保留扩容入口）；
 * - 需要不同配置（如多库）时声明新的 Pool，V1 不做实例级差异化配置。
 *
 * @note 依赖 DBService::pendingCount()（v1.5.1 新增）
 * @note 当前版本：v1.5.1
 */
class DBSERVICE_EXPORT DBServicePool : public QObject
{
    Q_OBJECT
public:
    /**
     * @param baseConfig 数据库配置（poolSize 个实例共享，自动克隆）
     * @param poolSize   实例数（<=0 时按 1 处理，即单实例用法）
     * @param type       服务策略类型（默认 Standard）
     * @param parent     父对象
     */
    explicit DBServicePool(const DBConfig& baseConfig, int poolSize,
                           DBServiceType type = DBServiceType::Standard,
                           QObject* parent = nullptr);
    ~DBServicePool() override;

    /** @brief 创建 poolSize 个 DBService 实例（克隆配置 + 自动命名 + 连接信号） */
    bool init();
    /** @brief 逐实例 start() */
    void start();

    /** @brief 分发任务：自动选择当前排队最少的实例 */
    void execSql(const QString& taskId, const QVector<SqlTuple>& sqlList);
    /** @brief 分发单条 SQL（便捷重载，内部构造 SqlTuple） */
    void execSql(const QString& taskId, const QString& tag, const QString& sql, bool isModify = false);

    /** @brief 定向分发：跳过负载均衡，固定发往第 instanceIndex 个实例 */
    void execSqlOn(int instanceIndex, const QString& taskId, const QVector<SqlTuple>& sqlList);

    /** @brief 池内实例总数 */
    int poolSize() const { return m_services.size(); }
    /** @brief 全池未完成任务数 */
    int pendingCount() const;
    /** @brief 单个实例未完成任务数（下标越界返回 -1） */
    int pendingCount(int index) const;
    /** @brief 实例名列表，如 {pool_0, pool_1, ...} */
    QStringList serviceNames() const;

signals:
    /** @brief 聚合转发：任何实例完成都会触发，result.serviceName 可溯源 */
    void sigExecFinished(const DBServiceResult& result);

private:
    /** @brief 负载均衡：返回当前排队任务最少的实例下标 */
    int pickInstance() const;
    /** @brief 连接第 idx 个实例的信号（信号到信号直连聚合） */
    void initConnections(int idx);

    DBConfig            m_baseConfig;
    int                 m_poolSize;
    DBServiceType       m_type;
    QVector<DBService*> m_services;
};

#endif // DBSERVICEPOOL_H
