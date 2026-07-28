#ifndef DBSERVICESTRUCT_H
#define DBSERVICESTRUCT_H

#include <QString>
#include <QJsonObject>
#include <QVariant>
#include <QVariantMap>
#include <QMap>
#include <QVector>
#include "DBConfig.h"

/**
 * @brief DBService服务类型枚举
 */
enum class DBServiceType
{
    Standard      = 0,
    Reliable      = 1,
    HighPerf      = 2,
    Unreliable    = 3
};

/**
 * @brief SQL元组结构体（用户输入的最小单元）
 */
struct SqlTuple
{
    QString tag;
    QString sql;
    bool    isModify;

    SqlTuple() : tag(""), sql(""), isModify(false) {}
    SqlTuple(const QString& t, const QString& s, bool m) : tag(t), sql(s), isModify(m) {}

    static SqlTuple create(const QString& tag, const QString& sql, bool isModify = false) {
        return SqlTuple(tag, sql, isModify);
    }
};

/**
 * @brief DBService返回结果结构体
 */
struct DBServiceResult
{
    bool isSuccess;
    QString serviceName;
    QString taskId;
    QVector<SqlTuple> sqlList;

    DBErrCode errCode;
    QString errMsg;
    QString errSql;
    int errTupleIndex;

    QMap<QString, QVariant> data;

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
