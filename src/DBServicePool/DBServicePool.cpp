#include "DBServicePool.h"
#include "dbservice.h"

#include <QDebug>

DBServicePool::DBServicePool(const DBConfig& baseConfig, int poolSize,
                             DBServiceType type, QObject* parent)
    : QObject(parent)
    , m_baseConfig(baseConfig)
    , m_poolSize(poolSize > 0 ? poolSize : 1)
    , m_type(type)
{
}

DBServicePool::~DBServicePool()
{
    // m_services 均为 QObject 子对象（parent=this），随本对象析构自动释放
}

bool DBServicePool::init()
{
    m_services.clear();
    for (int i = 0; i < m_poolSize; ++i) {
        DBService* svc = new DBService(m_type, m_baseConfig,
                                       TaskThreadModel::WorkerThread, this);
        svc->setServiceName(QStringLiteral("pool_%1").arg(i));
        m_services.append(svc);
        initConnections(i);
    }
    return !m_services.isEmpty();
}

void DBServicePool::start()
{
    for (DBService* svc : m_services)
        svc->start();
}

void DBServicePool::execSql(const QString& taskId, const QVector<SqlTuple>& sqlList)
{
    if (m_services.isEmpty()) {
        qWarning() << "DBServicePool: not initialized, call init() first";
        return;
    }
    m_services[pickInstance()]->onExecSqlList(sqlList, taskId);
}

void DBServicePool::execSql(const QString& taskId, const QString& tag,
                            const QString& sql, bool isModify)
{
    QVector<SqlTuple> sqlList;
    sqlList.append(SqlTuple(tag, sql, isModify));
    execSql(taskId, sqlList);
}

void DBServicePool::execSqlOn(int instanceIndex, const QString& taskId,
                              const QVector<SqlTuple>& sqlList)
{
    if (instanceIndex < 0 || instanceIndex >= m_services.size()) {
        qWarning() << "DBServicePool: instance index out of range:" << instanceIndex;
        return;
    }
    m_services[instanceIndex]->onExecSqlList(sqlList, taskId);
}

int DBServicePool::pendingCount() const
{
    int total = 0;
    for (const DBService* svc : m_services)
        total += svc->pendingCount();
    return total;
}

int DBServicePool::pendingCount(int index) const
{
    if (index < 0 || index >= m_services.size())
        return -1;
    return m_services[index]->pendingCount();
}

QStringList DBServicePool::serviceNames() const
{
    QStringList names;
    for (const DBService* svc : m_services)
        names.append(svc->serviceName());
    return names;
}

int DBServicePool::pickInstance() const
{
    int best = 0;
    for (int i = 1; i < m_services.size(); ++i) {
        if (m_services[i]->pendingCount() < m_services[best]->pendingCount())
            best = i;
    }
    return best;
}

void DBServicePool::initConnections(int idx)
{
    // 信号到信号直连：任何实例完成自动转发（DBService 当前仅此一个结果信号）
    connect(m_services[idx], &DBService::sigExecFinished,
            this, &DBServicePool::sigExecFinished);
}
