#ifndef SQLUNIT_H
#define SQLUNIT_H

#include <QString>

/**
 * @brief 单条SQL执行单元结构体
 * 最小执行粒度，一个对象对应一条SQL语句
 */
struct SqlUnit
{
    QString jsonKey;  ///< 查询结果在JSON中的顶层键名，用于解析路由
    QString sql;      ///< 待执行的原生SQL语句
    bool    isModify; ///< true=增/删/改语句  false=查询语句
    SqlUnit():jsonKey(""),sql(""),isModify(false)
    {}
    SqlUnit(const QString &key,const QString &your_sql,bool isSqlModify):jsonKey(key),sql(your_sql),isModify(isSqlModify)
    {}
    static SqlUnit createSqlUnit(const QString &key,const QString &your_sql,bool isSqlModify=false)
    {
        return  SqlUnit(key,your_sql,isSqlModify);
    }

};

#endif // SQLUNIT_H
