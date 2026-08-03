#ifndef DBTASKRESULT_H
#define DBTASKRESULT_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "dbtask.h"
#include "dbconfig.h"

/**
 * @brief 任务执行统一结果结构体
 * 规则：
 * 1. 任务执行失败时，resultJson 强制清空
 * 2. errUnitIndex 记录出错SqlUnit下标(0开始)，-1代表无错误
 */
struct DBTaskResult
{
    DBTask          task;          ///< 原始任务对象（透传），用于溯源
    QString         errMsg;        ///< 错误文字描述
    QString         errSql;        ///< 执行失败的SQL原文
    QJsonDocument   resultJson;    ///< 查询结果JSON，失败时强制清空
    DBErrCode       errCode;       ///< 标准化错误码
    int             errUnitIndex;  ///< 出错SqlUnit下标(从0开始)，-1=无错误
    bool            isSuccess;     ///< 任务整体执行状态：true=成功 false=失败

    DBTaskResult()
        : errCode(DBErrCode::SUCCESS)
        , errUnitIndex(-1)
        , isSuccess(false)
    {}

    /**
     * @brief 重置结果状态，恢复初始值
     */
    void reset()
    {
        isSuccess    = false;
        errCode      = DBErrCode::SUCCESS;
        errMsg.clear();
        errSql.clear();
        errUnitIndex = -1;
        resultJson = QJsonDocument();
    }
};

Q_DECLARE_METATYPE(DBTaskResult)

#endif // DBTASKRESULT_H