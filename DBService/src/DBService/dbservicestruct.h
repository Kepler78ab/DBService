#ifndef DBSERVICESTRUCT_H
#define DBSERVICESTRUCT_H

#include <QString>
#include <QJsonObject>
#include <QVariant>
#include <QVariantMap>
#include <QMap>
#include <QVector>
#include "../DbAccess/DbAccessStruct/dbconfig.h"

/**
 * @brief DBService服务类型枚举
 * @note 不同类型对应不同的内部配置策略
 */
enum class DBServiceType
{
    Standard      = 0,  ///< 标准模式：用户自定义重试策略，需传入完整DBTaskManagerConfig
    Reliable      = 1,  ///< 可靠模式：ForeverRetry，保证SQL正确时100%执行
    HighPerf      = 2,  ///< 高性能模式：批量入队+短间隔，适合高频场景
    Unreliable    = 3   ///< 不可靠模式：NoRetry，只执行一次，不重试
};

/**
 * @brief SQL元组结构体（用户输入的最小单元）
 * @note 用户只需关心 tag、sql、isModify 三个字段
 */
struct SqlTuple
{
    QString tag;       ///< 标识key（对应结果中的key）
    QString sql;       ///< SQL语句
    bool    isModify;  ///< true=增删改, false=查询

    SqlTuple() : tag(""), sql(""), isModify(false) {}
    SqlTuple(const QString& t, const QString& s, bool m) : tag(t), sql(s), isModify(m) {}

    /**
     * @brief 快速创建SqlTuple的静态方法
     */
    static SqlTuple create(const QString& tag, const QString& sql, bool isModify = false) {
        return SqlTuple(tag, sql, isModify);
    }
};

/**
 * @brief DBService返回结果结构体
 * @note 包含执行状态、原始输入、错误信息、转换后的数据
 */
struct DBServiceResult
{
    bool isSuccess;                           ///< 是否执行成功
    QString serviceName;                      ///< 服务名（如 "DB1"、"DB2"）
    QString taskId;                           ///< 任务ID（用于链路追踪）
    QVector<SqlTuple> sqlList;                ///< 用户输入的SQL列表（元组格式）

    DBErrCode errCode;                        ///< 错误码
    QString errMsg;                           ///< 错误信息
    QString errSql;                           ///< 失败的SQL语句
    int errTupleIndex;                        ///< 失败的SqlTuple下标（成功时为-1）

    QMap<QString, QVariant> data;             ///< extractData后的数据（成功时有值）

    DBServiceResult()
        : isSuccess(false)
        , taskId("")
        , errCode(DBErrCode::SUCCESS)
        , errMsg("")
        , errSql("")
        , errTupleIndex(-1)
        , data()
    {}
};

Q_DECLARE_METATYPE(SqlTuple)
Q_DECLARE_METATYPE(QVector<SqlTuple>)
Q_DECLARE_METATYPE(QVector<QVariantMap>)

/**
 * @brief DBService原始JSON结果结构体
 * @note 直传QJsonObject，跳过extractData的Qt类型转换
 *       sigRawResult信号使用此结构体
 */
struct DBServiceRawResult
{
    QString     taskId;        ///< 任务追踪ID
    QJsonObject resultJson;    ///< 原始查询结果JSON
    bool        isSuccess;     ///< 执行是否成功
    int         errCode;       ///< 错误码（DBErrCode枚举值）
    QString     errMsg;        ///< 错误描述
    QString     errSql;        ///< 出错SQL

    DBServiceRawResult()
        : isSuccess(false)
        , errCode(0)
    {}
};

Q_DECLARE_METATYPE(DBServiceRawResult)

#endif // DBSERVICESTRUCT_H
