#ifndef QDBCONNECTION_H
#define QDBCONNECTION_H

#include <QObject>
#include <QSqlDatabase>
#include <QJsonArray>
#include <QString>
#include <QTimer>
#include "../DbAccessStruct/dbstructs.h"

/**
 * @class QDBConnection
 * @brief 数据库连接与事务执行核心类
 * 功能：数据库初始化、事务任务串行执行、异常捕获与结果封装
 */
class QDBConnection : public QObject
{
    Q_OBJECT
public:
    explicit QDBConnection(QObject *parent = nullptr);
    ~QDBConnection() override;

    /**
     * @brief 初始化数据库连接
     * @param config 连接配置参数
     * @return 连接成功返回true，失败返回false
     */
    bool init(const DBConfig& config);

    /**
     * @brief 执行单个事务任务
     * @param task 待执行事务任务
     * @param outResult 输出：统一执行结果结构体
     */
    void execTask(const DBTask& task, DBTaskResult& outResult);

signals:
    /**
     * @brief 连接状态变化信号
     * @param isConnected true=已连接 false=未连接
     */
    void sigConnectionChanged(bool isConnected);

private slots:
    /**
     * @brief 重试连接定时器回调
     */
    void onRetryTimer();

    /**
     * @brief 实际执行连接操作
     */
    void doConnect();

private:
    /**
     * @brief 关闭数据库连接
     */
    void closeConnection();

    /**
     * @brief 执行单条SQL单元
     * @param unit SQL单元
     * @param resultObject 输出紧凑结果对象：
     *        - 查询: {"type":"query", "columns":[...], "rows":[[...]]}
     *        - 写:   {"type":"write", "affectedRows":n[, "lastInsertId":x]}
     * @return 执行成功返回true，失败返回false
     */
    bool executeSqlUnit(const SqlUnit& unit, QJsonObject& resultObject);

private:
    DBConfig        m_config;          ///< 数据库连接配置
    QSqlDatabase    m_db;              ///< 数据库连接对象
    bool            m_isConnected;     ///< 是否已连接到数据库
    QString         m_connectionName;  ///< 连接名称（用于唯一标识）
    QTimer*         m_retryTimer;     ///< 重试连接定时器
    int             m_retryCount;      ///< 当前重试次数
};

#endif // QDBCONNECTION_H