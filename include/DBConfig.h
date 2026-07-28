#ifndef DBCONFIG_H
#define DBCONFIG_H

#include <QString>
#include <QtGlobal>

/**
 * @brief 数据库驱动类型枚举
 */
enum class DBDriverType
{
    MySQL       = 0,
    Oracle      = 1,
    ODBC        = 2,
    PostgreSQL  = 3,
    SQLite      = 4,
    DB2         = 5
};

/**
 * @brief 数据库错误码枚举
 */
enum class DBErrCode
{
    SUCCESS        = 0,
    DB_NOT_OPEN    = 1,
    SQL_SYNTAX_ERR = 2,
    EXECUTE_FAILED = 3,
    TRANS_ERR      = 4
};

/**
 * @brief 数据库连接配置结构体
 */
struct DBConfig
{
    DBDriverType driverType;
    QString      host;
    quint16      port;
    QString      dbName;
    QString      user;
    QString      password;

    bool         enableRetry = false;
    int          retryTimes = 3;
    int          retryIntervalMs = 1000;
    bool         limitRetryCount = false;
};

#endif // DBCONFIG_H
