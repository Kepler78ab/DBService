#ifndef DBTASK_H
#define DBTASK_H

#include <QVector>
#include <QString>
#include <QtGlobal>
#include "sqlunit.h"
#include <QDateTime>
#include <QUuid>

/**
 * @brief 事务任务结构体
 * 
 * 一个 DBTask = 一个数据库原子事务
 * 内部所有SqlUnit串行执行，一错全回滚
 * 附带业务自定义字段，全链路透传
 * 
 * @note 扩展性设计：
 * - 如需添加业务自定义字段，请直接在结构体中添加成员变量
 * - 添加自定义字段后，需确保正确实现复制语义：
 *   1. 复制构造函数：`DBTask(const DBTask& other) = default;` 或手动实现
 *   2. 赋值运算符：`DBTask& operator=(const DBTask& other) = default;` 或手动实现
 * - 自定义字段会随任务在全链路中透传，可用于业务标识、日志追踪等场景
 * 
 * @example 扩展示例：
 * @code
 * struct DBTask {
 *     QVector<SqlUnit> sqlList;
 *     QString taskId;
 *     qint64 requestTime;
 *     int bizType;
 *     // 自定义扩展字段
 *     QString operatorId;      // 操作人ID
 *     QString traceId;        // 链路追踪ID
 *     QVariant extraData;     // 额外业务数据
 *     
 *     // 必须提供复制构造和赋值运算符
 *     DBTask(const DBTask&) = default;
 *     DBTask& operator=(const DBTask&) = default;
 * };
 * @endcode
 */
struct DBTask
{
    QVector<SqlUnit> sqlList;   ///< 当前事务包含的所有SQL单元集合
    QString          taskId;     ///< 任务唯一标识，用于链路追踪（UUID格式）
    qint64           requestTime = 0;///< 任务发起时间戳（毫秒）
    int              bizType = 0;    ///< 业务类型标识，区分不同业务场景（自定义枚举值）

    /**
     * @brief 默认构造函数
     */
    DBTask() = default;

    /**
     * @brief 复制构造函数
     * @note 默认实现使用成员逐一复制，添加自定义字段后建议保留此声明
     */
    DBTask(const DBTask&) = default;

    /**
     * @brief 赋值运算符
     * @note 默认实现使用成员逐一赋值，添加自定义字段后建议保留此声明
     * @return 返回当前对象引用
     */
    DBTask& operator=(const DBTask&) = default;

    /**
     * @brief 创建带有随机UUID的任务
     * @return 新创建的DBTask对象，taskId已自动生成
     */
    static DBTask spawnRandomQUuidTask()
    {
        DBTask t;
        t.taskId = QUuid::createUuid().toString();
        t.requestTime = QDateTime::currentMSecsSinceEpoch();
        return t;
    }

    /**
     * @brief 向任务中添加SQL单元
     * @param unit 要添加的SqlUnit对象
     */
    void append(const SqlUnit& unit)
    {
        sqlList.append(unit);
    }
};

Q_DECLARE_METATYPE(DBTask)

#endif // DBTASK_H
