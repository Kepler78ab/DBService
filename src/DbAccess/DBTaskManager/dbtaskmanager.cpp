#include "dbtaskmanager.h"
#include "../QDBConnection/qdbconnection.h"
#include <QDateTime>
#include <QDebug>
#include <QException>

int g_reliableMaxRetryDurationMs = 60000;

DBTaskManager::DBTaskManager(QObject *parent)
    : QObject(parent)
    , m_dbConn(nullptr)
    , m_taskTimer(new QTimer(this))
    , m_delayTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this))
    , m_batchTimer(new QTimer(this))
    , m_isProcessing(false)
{
    // 配置所有定时器为单次触发模式
    m_taskTimer->setSingleShot(true);
    m_delayTimer->setSingleShot(true);
    m_retryTimer->setSingleShot(true);
    m_batchTimer->setSingleShot(true);

    // 连接定时器信号到槽函数
    connect(m_taskTimer, &QTimer::timeout, this, &DBTaskManager::onTaskLoop);
    connect(m_delayTimer, &QTimer::timeout, this, &DBTaskManager::onTaskDelayTimeout);
    connect(m_retryTimer, &QTimer::timeout, this, &DBTaskManager::onRetryDelayTimeout);
    connect(m_batchTimer, &QTimer::timeout, this, &DBTaskManager::onBatchEnqueueTimeout);
}

bool DBTaskManager::init(const DBTaskManagerConfig& config)
{
    try {
        m_config = config;

        // 创建数据库连接实例，指定父对象由Qt管理内存
        m_dbConn = new QDBConnection(this);
        if (!m_dbConn->init(config.dbConnConfig))
        {
            qWarning() << "Failed to initialize database connection";
            return false;
        }

        // 如果启用批量入队，初始化批量入队定时器
        if (m_config.enableBatchEnqueue)
        {
            qDebug() << "Batch enqueue enabled, interval:" << m_config.batchIntervalMs 
                     << "ms, max size:" << m_config.batchMaxSize;
        }

        return true;
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::init:" << e.what();
        return false;
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::init";
        return false;
    }
}

void DBTaskManager::start()
{
    try {
        if (!m_taskTimer->isActive())
        {
            m_taskTimer->start(0);
        }
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::start:" << e.what();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::start";
    }
}

bool DBTaskManager::pushTask(const DBTask& task)
{
    try {
        QMutexLocker locker(&m_mutex);

        // 如果启用批量入队模式，先放入缓冲区
        if (m_config.enableBatchEnqueue)
        {
            // 检查缓冲区是否已满
            if (m_batchBuffer.size() >= m_config.batchMaxSize)
            {
                // 缓冲区已满，立即执行批量入队
                flushBatchBuffer();
            }

            // 将任务放入缓冲区（共享指针，零拷贝）
            auto batchTask = std::make_shared<DBTask>(task);
            if (batchTask->requestTime <= 0) {
                batchTask->requestTime = QDateTime::currentMSecsSinceEpoch();
            }
            m_batchBuffer.enqueue(batchTask);
            qDebug() << "Task added to batch buffer:" << batchTask->taskId 
                     << ", buffer size:" << m_batchBuffer.size();

            // 如果定时器未激活，启动批量入队定时器
            if (!m_batchTimer->isActive())
            {
                m_batchTimer->start(m_config.batchIntervalMs);
            }

            return true;
        }

        // 非批量模式：直接入队
        // 检查队列是否已满
        if (m_taskQueue.size() >= m_config.queueMaxSize)
        {
            if (m_config.queueFullPolicy == QueueFullPolicy::REJECT)
            {
                qWarning() << "Task queue is full, rejecting new task:" << task.taskId;
                return false;
            }
            else
            {
                qWarning() << "Task queue is full (BLOCK mode), rejecting task:" << task.taskId;
                return false;
            }
        }

        // 创建任务节点，初始化错误信息字段
        TaskNode node;
        node.task = std::make_shared<DBTask>(task);
        if (node.task->requestTime <= 0) {
            node.task->requestTime = QDateTime::currentMSecsSinceEpoch();
        }
        node.currentRetry = 0;
        node.lastErrMsg.clear();
        node.lastErrSql.clear();
        node.lastErrIndex = -1;
        node.lastErrCode = DBErrCode::SUCCESS;

        // FIFO: 新任务追加到队尾
        m_taskQueue.enqueue(node);

        qInfo() << "[DBTaskManager] 任务入队 taskId=" << task.taskId
                << "queueDepth=" << m_taskQueue.size();

        // 如果当前没有任务在处理，启动调度
        if (!m_isProcessing)
        {
            m_taskTimer->start(0);
        }

        return true;
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::pushTask:" << e.what();
        return false;
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::pushTask";
        return false;
    }
}

void DBTaskManager::onPushTask(const DBTask& task)
{
    try {
        // 槽函数接口：直接调用 pushTask
        pushTask(task);
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::onPushTask:" << e.what();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::onPushTask";
    }
}

void DBTaskManager::onBatchEnqueueTimeout()
{
    try {
        QMutexLocker locker(&m_mutex);
        flushBatchBuffer();
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::onBatchEnqueueTimeout:" << e.what();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::onBatchEnqueueTimeout";
    }
}

void DBTaskManager::flushBatchBuffer()
{
    try {
        if (m_batchBuffer.isEmpty())
        {
            return;
        }

        qDebug() << "Flushing batch buffer, count:" << m_batchBuffer.size();

        // 将缓冲区中的任务批量转移到任务队列
        while (!m_batchBuffer.isEmpty())
        {
            auto taskPtr = m_batchBuffer.dequeue();

            // 检查任务队列是否已满
            if (m_taskQueue.size() >= m_config.queueMaxSize)
            {
                if (m_config.queueFullPolicy == QueueFullPolicy::REJECT)
                {
                    qWarning() << "Task queue is full, dropping task:" << taskPtr->taskId;
                    continue;
                }
                else
                {
                    qWarning() << "Task queue is full, dropping task:" << taskPtr->taskId;
                    continue;
                }
            }

            // 创建任务节点（共享指针直接传递，零拷贝）
            TaskNode node;
            node.task = taskPtr;
            if (node.task->requestTime <= 0) {
                node.task->requestTime = QDateTime::currentMSecsSinceEpoch();
            }
            node.currentRetry = 0;
            node.lastErrMsg.clear();
            node.lastErrSql.clear();
            node.lastErrIndex = -1;
            node.lastErrCode = DBErrCode::SUCCESS;

            m_taskQueue.enqueue(node);
        }

        qDebug() << "Batch flush completed, queue size:" << m_taskQueue.size();

        // 如果当前没有任务在处理，启动调度
        if (!m_isProcessing && !m_taskQueue.isEmpty())
        {
            m_taskTimer->start(0);
        }
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::flushBatchBuffer:" << e.what();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::flushBatchBuffer";
    }
}

void DBTaskManager::onTaskLoop()
{
    try {
        TaskNode node;

        {
            QMutexLocker locker(&m_mutex);

            // 队列为空，直接返回
            if (m_taskQueue.isEmpty())
            {
                return;
            }

            // 正在处理中，等待当前任务完成
            if (m_isProcessing)
            {
                return;
            }

            // FIFO: 从队头取出任务
            node = m_taskQueue.dequeue();
            m_isProcessing = true;
        }

        // 检查是否需要延时执行（仅首次执行时延时，重试不延时）
        if (m_config.enableTaskDelay && node.currentRetry == 0)
        {
            m_currentTask = node;
            m_delayTimer->start(m_config.taskDelayMs);
        }
        else
        {
            processTask(node);
        }
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::onTaskLoop:" << e.what();
        m_isProcessing = false;
        scheduleNextTask();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::onTaskLoop";
        m_isProcessing = false;
        scheduleNextTask();
    }
}

void DBTaskManager::onTaskDelayTimeout()
{
    try {
        processTask(m_currentTask);
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::onTaskDelayTimeout:" << e.what();
        m_isProcessing = false;
        scheduleNextTask();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::onTaskDelayTimeout";
        m_isProcessing = false;
        scheduleNextTask();
    }
}

void DBTaskManager::onRetryDelayTimeout()
{
    try {
        processTask(m_currentTask);
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::onRetryDelayTimeout:" << e.what();
        m_isProcessing = false;
        scheduleNextTask();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::onRetryDelayTimeout";
        m_isProcessing = false;
        scheduleNextTask();
    }
}

void DBTaskManager::processTask(const TaskNode& node)
{
    try {
        DBTaskResult result;

        // 调用下层数据库连接执行任务
        m_dbConn->execTask(*node.task, result);

        if (!result.isSuccess)
        {
            // 判断永久性错误：跳过重试
            // 1) SQL_SYNTAX_ERR → 语法错误，永不能成功
            // 2) 同一 SqlUnit 下标重复失败，且不是连接级事务错误（TRANS_ERR）
            bool isPermanent = false;

             if (result.errCode == DBErrCode::SQL_SYNTAX_ERR) {
                 isPermanent = true;
             } else if (result.errCode != DBErrCode::TRANS_ERR &&
                        result.errUnitIndex >= 0 &&
                        node.lastErrIndex >= 0 &&
                        node.lastErrIndex == result.errUnitIndex) {
                isPermanent = true;
            }

            if (isPermanent) {
                qWarning() << "Task" << node.task->taskId
                           << "encountered permanent error at index" << result.errUnitIndex
                           << ", skipping retry. Error:" << result.errMsg;

                DBTaskResult finalResult = result;
                if (finalResult.errMsg.isEmpty()) {
                    finalResult.errMsg = QString("Permanent error: SqlUnit index %1 failed again")
                                         .arg(result.errUnitIndex);
                }

                m_isProcessing = false;
                emit sigTaskComplete(finalResult);
                scheduleNextTask();
                return;
            }

            // 执行失败，保存错误信息并调度重试逻辑
            TaskNode retryNode = node;

            // 保存当前失败的详细信息
            retryNode.lastErrMsg = result.errMsg;
            retryNode.lastErrSql = result.errSql;
            retryNode.lastErrIndex = result.errUnitIndex;
            retryNode.lastErrCode = result.errCode;

            qDebug() << "Task" << node.task->taskId << "failed at SqlUnit index"
                     << result.errUnitIndex << ", error:" << result.errMsg;

            scheduleRetry(retryNode);
        }
        else
        {
            // 执行成功，标记空闲并通知上层
            m_isProcessing = false;
            emit sigTaskComplete(result);

            // 调度处理下一个任务
            scheduleNextTask();
        }
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::processTask:" << e.what();
        
        // 构造失败结果并通知上层
        m_isProcessing = false;
        
        DBTaskResult result;
        result.reset();
        result.task = *node.task;
        result.isSuccess = false;
        result.errCode = DBErrCode::EXECUTE_FAILED;
        result.errMsg = QString("Exception in processTask: ") + e.what();
        result.errUnitIndex = -1;
        result.resultJson = QJsonDocument();
        
        emit sigTaskComplete(result);
        scheduleNextTask();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::processTask";
        
        // 构造失败结果并通知上层
        m_isProcessing = false;
        
        DBTaskResult result;
        result.reset();
        result.task = *node.task;
        result.isSuccess = false;
        result.errCode = DBErrCode::EXECUTE_FAILED;
        result.errMsg = "Unknown exception in processTask";
        result.errUnitIndex = -1;
        result.resultJson = QJsonDocument();
        
        emit sigTaskComplete(result);
        scheduleNextTask();
    }
}

void DBTaskManager::scheduleRetry(const TaskNode& node)
{
    try {
        bool shouldRetry = false;
        TaskNode retryNode = node;

        if (m_config.retryMode == TaskRetryMode::ForeverRetry)
        {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (retryNode.task->requestTime <= 0) {
                retryNode.task->requestTime = nowMs;
            }
            const qint64 elapsedMs = nowMs - retryNode.task->requestTime;
            if (elapsedMs >= g_reliableMaxRetryDurationMs)
            {
                m_isProcessing = false;

                DBTaskResult result;
                result.reset();
                result.task = *node.task;
                result.isSuccess = false;
                result.errCode = node.lastErrCode;
                result.errMsg = QString("Reliable retry timeout after %1 ms: %2")
                               .arg(elapsedMs).arg(node.lastErrMsg);
                result.errSql = node.lastErrSql;
                result.errUnitIndex = node.lastErrIndex;
                result.resultJson = QJsonDocument();

                qWarning() << "Task" << node.task->taskId
                           << "reliable retry timeout after" << elapsedMs
                           << "ms, dropping current task and scheduling next.";

                emit sigTaskComplete(result);
                scheduleNextTask();
                return;
            }
        }

        switch (m_config.retryMode)
        {
        case TaskRetryMode::NoRetry:
            shouldRetry = false;
            break;

        case TaskRetryMode::LimitedRetry:
            if (retryNode.currentRetry < m_config.maxRetryCount)
            {
                retryNode.currentRetry++;
                shouldRetry = true;
            }
            else
            {
                shouldRetry = false;
            }
            break;

        case TaskRetryMode::ForeverRetry:
            retryNode.currentRetry++;
            shouldRetry = true;
            break;
        }

        if (shouldRetry)
        {
            // 需要重试，使用定时器延时
            m_currentTask = retryNode;
            qDebug() << "Scheduling retry" << retryNode.currentRetry << "for task:"
                     << node.task->taskId << ", last failed at index:" << node.lastErrIndex;
            m_retryTimer->start(m_config.retryIntervalMs);
        }
        else
        {
            // 不需要重试，构造失败结果并通知上层
            m_isProcessing = false;

            DBTaskResult result;
            result.reset();
            result.task = *node.task;
            result.isSuccess = false;
            // 使用最后一次失败的错误信息
            result.errCode = node.lastErrCode;
            result.errMsg = QString("Task failed after %1 retries: %2")
                           .arg(node.currentRetry).arg(node.lastErrMsg);
            result.errSql = node.lastErrSql;
            result.errUnitIndex = node.lastErrIndex;
            result.resultJson = QJsonDocument();

            qDebug() << "Task" << node.task->taskId << "final failure after"
                     << node.currentRetry << "retries, failed at index:" << node.lastErrIndex;

            emit sigTaskComplete(result);

            // 调度处理下一个任务
            scheduleNextTask();
        }
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::scheduleRetry:" << e.what();
        m_isProcessing = false;
        scheduleNextTask();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::scheduleRetry";
        m_isProcessing = false;
        scheduleNextTask();
    }
}

void DBTaskManager::scheduleNextTask()
{
    try {
        QMutexLocker locker(&m_mutex);

        // 队列中还有任务，调度下一个
        if (!m_taskQueue.isEmpty())
        {
            m_taskTimer->start(0);
        }
    } catch (const QException& e) {
        qCritical() << "Exception in DBTaskManager::scheduleNextTask:" << e.what();
    } catch (...) {
        qCritical() << "Unknown exception in DBTaskManager::scheduleNextTask";
    }
}
