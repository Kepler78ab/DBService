#ifndef DBSERVICESTRUCT_H
#define DBSERVICESTRUCT_H

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QVariantMap>
#include <QMap>
#include <QVector>
#include "../DbAccess/DbAccessStruct/dbconfig.h"

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
 * @brief DBService原始JSON结果结构体（轻量版，用于兼容旧调用方）
 */
struct DBServiceRawResult
{
    QString         taskId;
    QString         serviceName;
    QJsonDocument   resultJson;
    bool            isSuccess;
    DBErrCode       errCode;
    QString         errMsg;
    QString         errSql;
    int             errTupleIndex;

    DBServiceRawResult()
        : isSuccess(false)
        , errCode(DBErrCode::SUCCESS)
        , errTupleIndex(-1)
    {}
};

/**
 * @brief DBService返回结果结构体（完整版）
 * @note 字段按 8 字节成员在前、小字段集中尾部排列，减少 padding
 */
struct DBServiceResult
{
    QString serviceName;
    QString taskId;
    QVector<SqlTuple> sqlList;

    QString errMsg;
    QString errSql;

    QJsonDocument rawJson;
    QMap<QString, QVariant> data;

    DBErrCode errCode;
    int errTupleIndex;
    bool isSuccess;

    DBServiceResult()
        : errCode(DBErrCode::SUCCESS)
        , errTupleIndex(-1)
        , isSuccess(false)
        , taskId("")
        , errMsg("")
        , errSql("")
        , data()
    {}

    DBServiceRawResult toRawResult() const {
        DBServiceRawResult raw;
        raw.taskId        = this->taskId;
        raw.serviceName   = this->serviceName;
        raw.resultJson    = this->rawJson;
        raw.isSuccess     = this->isSuccess;
        raw.errCode       = this->errCode;
        raw.errMsg        = this->errMsg;
        raw.errSql        = this->errSql;
        raw.errTupleIndex = this->errTupleIndex;
        return raw;
    }
};

Q_DECLARE_METATYPE(SqlTuple)
Q_DECLARE_METATYPE(QVector<SqlTuple>)
Q_DECLARE_METATYPE(QVector<QVariantMap>)
Q_DECLARE_METATYPE(DBServiceRawResult)
Q_DECLARE_METATYPE(DBServiceResult)

#endif // DBSERVICESTRUCT_H
