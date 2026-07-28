#ifndef DBTASKMANAGER_H
#define DBTASKMANAGER_H

#include <QObject>
#include <QQueue>
#include <QTimer>
#include <QMutex>
#include "../DbAccessStruct/dbstructs.h"

extern int g_reliableMaxRetryDurationMs;

/**
 * @brief 任务失败重试策略枚举
 */
enum class TaskRetryMode
{
    NoRetry,        ///< 不进行重试
    LimitedRetry,   ///< 有限次数重试
    ForeverRetry    ///< 永久循环重试
};

/**
 * @brief 任务队列已满时的处理策略枚举
 */
enum class QueueFullPolicy
{
    BLOCK,  ///< 阻塞当前投递线程，直到队列腾出空间
    REJECT  ///< 直接拒绝新任务，返回入队失败
};

/**
 * @brief 任务管理器全局配置结构体
 * 整合队列、重试、延时、数据库全套配置
 */
struct DBTaskManagerConfig
{
    int             queueMaxSize = 100;     ///< 任务队列最大容量
    QueueFullPolicy queueFullPolicy = QueueFullPolicy::BLOCK;  ///< 队列满处理策略
    TaskRetryMode   retryMode = TaskRetryMode::NoRetry;        ///< 任务重试模式
    int             maxRetryCount = 3;    ///< 最大重试次数
    int             retryIntervalMs = 5000;  ///< 重试间隔(毫秒)
    bool            enableTaskDelay = false;  ///< 是否开启任务延时执行
    int             taskDelayMs = 200;      ///< 任务延时时长(毫秒)

    bool            enableBatchEnqueue = false;  ///< 是否开启批量入队
    int             batchIntervalMs = 1000;     ///< 批量入队间隔(毫秒)
    int             batchMaxSize = 10;        ///< 单次最大批量大小

    DBConfig        dbConnConfig;     ///< 内嵌数据库连接配置
};

/**
 * @brief 队列存储节点结构体
 * 封装原始任务 + 当前重试次数 + 上次失败信息，用于队列缓存
 */
struct TaskNode
{
    DBTask    task;          ///< 原始事务任务
    int       currentRetry;  ///< 当前已重试次数，默认值为0
    QString   lastErrMsg;    ///< 上次失败的错误信息
    QString   lastErrSql;    ///< 上次失败的SQL语句
    int       lastErrIndex;  ///< 上次失败的SqlUnit下标(-1表示无错误)
    DBErrCode lastErrCode;   ///< 上次失败的错误码
};

class QDBConnection;

/**
 * @class DBTaskManager
 * @brief 异步数据库任务调度管理器
 * 特性：队列串行执行、定时器异步调度、FIFO顺序保障、重试不插队
 */
class DBTaskManager : public QObject
{
    Q_OBJECT
public:
    explicit DBTaskManager(QObject *parent = nullptr);
    ~DBTaskManager() override = default;

    /**
     * @brief 初始化任务管理器
     * @param config 管理器全套配置
     * @return 初始化成功返回true
     */
    bool init(const DBTaskManagerConfig& config);

    /**
     * @brief 启动任务调度循环
     */
    Q_INVOKABLE void start();

    /**
     * @brief 同步直投接口
     * 外部直接调用，投递事务任务
     * @param task 待执行DB事务任务
     * @return true=入队成功  false=入队失败
     */
    bool pushTask(const DBTask& task);

signals:
    /**
     * @brief 任务执行完成信号
     * 成功/失败都会触发，唯一出参：统一结果结构体
     */
    void sigTaskComplete(const DBTaskResult& res);

public slots:
    /**
     * @brief 槽函数接口：接收外部信号触发任务入队
     * @param task 待执行DB事务任务
     */
    Q_INVOKABLE void onPushTask(const DBTask& task);

private slots:
    /**
     * @brief 主任务调度循环槽函数
     */
    void onTaskLoop();

    /**
     * @brief 任务延时结束回调槽函数
     */
    void onTaskDelayTimeout();

    /**
     * @brief 任务重试延时结束回调槽函数
     */
    void onRetryDelayTimeout();

    /**
     * @brief 批量入队定时器触发槽函数
     */
    void onBatchEnqueueTimeout();

private:
    /**
     * @brief 处理单个任务
     */
    void processTask(const TaskNode& node);

    /**
     * @brief 调度任务重试
     */
    void scheduleRetry(const TaskNode& node);

    /**
     * @brief 继续处理队列中的下一个任务
     */
    void scheduleNextTask();

    /**
     * @brief 刷新批量入队缓冲区
     * 将缓冲区中的任务批量转移到任务队列
     */
    void flushBatchBuffer();

private:
    QQueue<TaskNode>      m_taskQueue;       ///< 任务队列（FIFO）
    QQueue<DBTask>        m_batchBuffer;     ///< 批量入队缓冲区
    QDBConnection*        m_dbConn;          ///< 数据库连接实例
    DBTaskManagerConfig   m_config;          ///< 管理器配置
    QMutex                m_mutex;           ///< 队列操作互斥锁
    QTimer*               m_taskTimer;        ///< 任务调度定时器
    QTimer*               m_delayTimer;       ///< 任务延时定时器
    QTimer*               m_retryTimer;      ///< 重试延时定时器
    QTimer*               m_batchTimer;      ///< 批量入队定时器
    TaskNode              m_currentTask;     ///< 当前正在处理的任务
    bool                  m_isProcessing;    ///< 是否正在处理任务
};

#endif // DBTASKMANAGER_H
