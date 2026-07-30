#ifndef DBSERVICESTRUCT_H
#define DBSERVICESTRUCT_H

#include <QString>
#include <QJsonDocument>
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
 * @brief DBService原始JSON结果结构体（轻量版，用于兼容旧调用方）
 * @note 完整信息请使用 DBServiceResult，通过 toRawResult() 获取此结构体
 */
struct DBServiceRawResult
{
    QString         taskId;        ///< 任务追踪ID
    QString         serviceName;   ///< 服务名称
    QJsonDocument   resultJson;    ///< 原始查询结果JSON（零转换）
    bool            isSuccess;     ///< 执行是否成功
    DBErrCode       errCode;       ///< 错误码
    QString         errMsg;        ///< 错误描述
    QString         errSql;        ///< 出错SQL
    int             errTupleIndex; ///< 出错SqlUnit下标(-1表示无错误)

    DBServiceRawResult()
        : isSuccess(false)
        , errCode(DBErrCode::SUCCESS)
        , errTupleIndex(-1)
    {}
};

/**
 * @brief DBService返回结果结构体（完整版）
 * @note 包含全量元信息 + 原始JSON。
 *       data 字段默认为空，需要结构化数据时请调用 DBService::extractData(rawJson)
 *       使用 toRawResult() 可获取轻量版 DBServiceRawResult 用于兼容旧调用方
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

    QJsonDocument rawJson;                         ///< 原始查询结果JSON（零转换）
    QMap<QString, QVariant> data;                  ///< 结构化数据（默认空，按需调用 extractData）

    DBServiceResult()
        : isSuccess(false)
        , taskId("")
        , errCode(DBErrCode::SUCCESS)
        , errMsg("")
        , errSql("")
        , errTupleIndex(-1)
        , data()
    {}

    /**
     * @brief 转换为轻量版 RawResult（用于兼容旧调用方）
     */
    DBServiceRawResult toRawResult() const {
        DBServiceRawResult raw;
        raw.taskId        = this->taskId;
        raw.serviceName   = this->serviceName;
        raw.resultJson    = this->rawJson;      // QJsonDocument 隐式共享
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
