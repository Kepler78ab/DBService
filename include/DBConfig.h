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
 * @note 字段按 8 字节成员在前、小字段集中尾部排列，减少 padding
 */
struct DBConfig
{
    QString      host;            ///< 数据库地址
    QString      dbName;          ///< 数据库名
    QString      user;            ///< 登录用户名
    QString      password;        ///< 登录密码

    DBDriverType driverType;      ///< 数据库驱动类型
    int          retryTimes = 3;      ///< 最大重试次数
    int          retryIntervalMs = 1000; ///< 重试间隔(毫秒)
    quint16      port;            ///< 端口号
    bool         enableRetry = false;   ///< 是否开启连接重试
    bool         limitRetryCount = false; ///< 是否限制重试次数
};

#endif // DBCONFIG_H
