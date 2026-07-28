#ifndef DBSERVICE_H
#define DBSERVICE_H

#include <QObject>
#include <QSettings>
#include <QPair>
#include <QThread>
#include <QMutex>
#include "dbservicestruct.h"
#include "../DbAccess/DbAccessStruct/dbtaskresult.h"
#include "dbservice_global.h"

struct DBTaskManagerConfig;
class DBTaskManager;

/**
 * @brief 任务管理器线程模式
 * 
 * MainThread  — 旧行为：TaskManager 在主线程，SQL 执行会阻塞 UI
 * WorkerThread — 新行为：TaskManager 移入子线程，不阻塞 UI
 */
enum class TaskThreadModel
{
    MainThread,
    WorkerThread
};

/**
 * @brief 数据库服务上层封装类
 * @note 提供极简API，屏蔽底层TaskManager细节
 *
 * 支持四种服务类型：
 * - Reliable：ForeverRetry，保证SQL正确时100%执行
 * - HighPerf：批量入队+短间隔，适合高频场景
 * - Standard：有限重试，折中策略
 * - Unreliable：NoRetry，只执行一次
 *
 * @note 当前版本：v1.2.0
 */
class DBSERVICE_EXPORT DBService : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数（仅创建对象，需后续调用init）
     * @param model 线程模式（默认主线程，旧行为）
     * @param parent 父对象
     */
    explicit DBService(TaskThreadModel model = TaskThreadModel::MainThread, QObject* parent = nullptr);

    /**
     * @brief 构造函数（服务类型+数据库配置）
     * @param type 服务类型枚举
     * @param dbConfig 数据库连接配置
     * @param model 线程模式（默认主线程，旧行为）
     * @param parent 父对象
     */
    explicit DBService(DBServiceType type, const DBConfig& dbConfig,
                       TaskThreadModel model = TaskThreadModel::MainThread, QObject* parent = nullptr);

    /**
     * @brief 构造函数（适配QPair，可直接使用loadSimpleConfig返回值）
     * @param simpleConfig QPair<DBConfig, DBServiceType>（由loadSimpleConfig返回）
     * @param model 线程模式（默认主线程，旧行为）
     * @param parent 父对象
     */
    explicit DBService(const QPair<DBConfig, DBServiceType>& simpleConfig,
                       TaskThreadModel model = TaskThreadModel::MainThread, QObject* parent = nullptr);

    ~DBService();

    /**
     * @brief 获取当前 DLL 版本号
     * @return "v1.1.0"
     */
    static QString currentVersion();

    /**
     * @brief 获取当前线程模式
     * @return TaskThreadModel 枚举值
     */
    TaskThreadModel threadModel() const { return m_threadModel; }

    /**
     * @brief 检查是否运行在子线程模式
     * @return true=子线程模式
     */
    bool isWorkerThread() const { return m_threadModel == TaskThreadModel::WorkerThread; }
    /**
     * @brief 初始化服务（用于默认构造后手动初始化）
     * @param type 服务类型
     * @param dbConfig 数据库配置
     * @return 是否初始化成功
     */
    bool init(DBServiceType type, const DBConfig& dbConfig);

    /**
     * @brief 初始化服务（适配QPair）
     * @param simpleConfig QPair<DBConfig, DBServiceType>
     * @return 是否初始化成功
     */
    bool init(const QPair<DBConfig, DBServiceType>& simpleConfig);

    /**
     * @brief 启动服务
     */
    void start();

    /**
     * @brief 加载简易配置文件（用于Reliable/HighPerf模式）
     * @param filePath 配置文件路径
     * @return QPair<DBConfig, DBServiceType> 数据库配置+服务类型
     */
    static QPair<DBConfig, DBServiceType> loadSimpleConfig(const QString& filePath);

    /**
     * @brief 获取服务名称
     */
    QString serviceName() const { return m_serviceName; }

    /**
     * @brief 设置服务名称
     */
    void setServiceName(const QString& name) { m_serviceName = name; }

public slots:
    /**
     * @brief 执行SQL列表（内部自动生成taskId）
     * @param sqlList SQL元组列表
     * @note 内部自动创建DBTask（随机UUID）并投递到TaskManager
     */
    void onExecSqlList(const QVector<SqlTuple>& sqlList);

    /**
     * @brief 执行SQL列表（指定taskId，用于请求追踪匹配）
     * @param sqlList SQL元组列表
     * @param taskId 调用方指定的任务标识（如IPC requestId），DBServiceResult原样带回
     * @note 不生成随机UUID，直接使用传入的taskId，DBBridgeServer等框架层使用
     */
    void onExecSqlList(const QVector<SqlTuple>& sqlList, const QString& taskId);

signals:
    /**
     * @brief 执行完成信号（转换后的结果）
     * @param result DBServiceResult结果
     * @note 业务层常规使用的信号，data字段已通过extractData转换
     */
    void sigExecFinished(const DBServiceResult& result);

    /**
     * @brief 原始JSON结果信号（跳过extractData转换）
     * @param rawResult 原始JSON结果结构体
     * @note DBBridgeServer等框架层使用，直传QJsonObject，零转换
     */
    void sigRawResult(const DBServiceRawResult& rawResult);

private slots:
    /**
     * @brief 内部槽：接收TaskManager完成信号
     * @param result DBTaskResult原始结果
     */
    void onTaskManagerComplete(const DBTaskResult& result);

private:
    /**
     * @brief 从DBTaskResult提取数据转换为QMap格式（内部使用）
     */
    static QMap<QString, QVariant> extractData(const DBTaskResult& result);

    /**
     * @brief 根据服务类型生成默认配置
     * @param type 服务类型
     * @param dbConfig 数据库配置
     * @return DBTaskManagerConfig 完整配置
     */
    DBTaskManagerConfig generateConfig(DBServiceType type, const DBConfig& dbConfig);

    /**
     * @brief 内部初始化（使用完整配置，由 public init 调用）
     * @param config 完整的TaskManager配置
     * @return 是否初始化成功
     */
    bool init(const DBTaskManagerConfig& config);

    /**
     * @brief 将DBTaskResult转换为DBServiceResult
     * @param taskResult DBTaskResult原始结果
     * @param originalTupleList 用户输入的原始SqlTuple列表
     * @return DBServiceResult 转换后的结果
     */
    DBServiceResult convertResult(const DBTaskResult& taskResult, const QVector<SqlTuple>& originalTupleList);

private:
    DBServiceType m_serviceType;                ///< 服务类型
    TaskThreadModel m_threadModel;              ///< 线程模式
    DBTaskManager* m_taskManager;               ///< 内部持有的任务管理器
    QThread* m_workerThread = nullptr;          ///< 工作线程（WorkerThread 模式下使用）
    QMutex m_pendingMutex;                      ///< 待处理任务映射互斥锁
    QMap<QString, QVector<SqlTuple>> m_pendingTasks;  ///< 待处理的任务映射（taskId -> SqlTupleList）
    QString m_serviceName;                      ///< 服务名称，由 DBServiceManager 在注册时设置
};

#endif // DBSERVICE_H