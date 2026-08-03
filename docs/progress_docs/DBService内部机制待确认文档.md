# DBService 内部机制待确认文档

> **文档目的**：DBService 是预编译 DLL（`server/3rdparty/DBService/`），当前仓库只有 `include/` 头文件与 `bin|lib/` 二进制，无源码。本文档列出需要**源码分析者**逐项确认的内部机制问题，确认结果将用于评估服务端的资源管理与故障处理策略。

## 1. 背景

服务端 `main.cpp` 以连接池模型创建 N 个 `DBService` 实例，`DBBridgeServer::dispatchRequest()` 将 SQL 任务按繁忙度调度到某个 DBService。DBService 在**独立工作线程**执行 SQL，完成后通过 `sigExecFinished` 信号回传结果。

分析者的输出应能回答本仓库其他模块对 DBService 的假设是否成立（见第 3 节）。

## 2. 源码位置说明

- 当前仓库仅有：`server/3rdparty/DBService/include/`（DBService.h / DBConfig.h / DBServiceStruct.h 等）与 `bin|lib/` 下预编译产物
- 源码不在本仓库，需向模块维护者获取或从版本库其他位置查找
- 阅读入口建议：`DBService.h`（类结构、信号槽）→ `DBServiceStruct.h`（数据结构）→ 实现文件（线程模型、SQL 执行、资源释放）

## 3. 待确认问题清单

### 3.1 工作线程模型

- [ ] `DBService::start()` 内部如何创建/管理工作线程？是 `QThread` 还是 `QtConcurrent`？线程数是否固定？
- [ ] 任务队列（若有）如何实现？`onExecSqlList` 是立即执行还是入队？
- [ ] `TaskThreadModel::WorkerThread` 枚举的含义，与其他模型（如有）的区别
- [ ] 工作线程与主线程的信号连接方式（AutoConnection / DirectConnection / QueuedConnection）？

### 3.2 SQL 执行与回调保证

- [ ] **SQL 执行挂起时**（如 MySQL 连接卡死、锁等待超时）：是否最终会发出 `sigExecFinished`？还是有超时机制强制回调？
- [ ] SQL 执行异常（语法错误、表不存在、连接断开）时：`DBServiceResult` 的 `isSuccess` / `errCode` 如何填充？是否一定回调？
- [ ] `sigExecFinished` 是否保证**每个 `onExecSqlList` 调用恰好对应一次回调**？极端情况下是否会丢失回调（导致 `DBBridgeServer` 的 `m_pendingCount` 永久挂起）？
- [ ] 大批量数据回传走 `sigRawResultFinished`（或类似信号）的触发条件与数据生命周期

### 3.3 启动/停止与线程退出

- [ ] `DBService::start()` 失败时的行为（返回 false？抛出？）
- [ ] **`stop()` / 析构时工作线程如何退出**？是否有 `quit()` + `wait()` 的等待逻辑？
- [ ] 服务端关闭（软重启 / 退出）时：工作线程中正在执行的 SQL 如何处理？是等待完成还是强制中断？
- [ ] 若 SQL 挂起，`stop()` 是否有超时强制退出机制？（这决定软重启时旧进程能否及时结束）

### 3.4 连接池与驱动资源

- [ ] 数据库连接（QSqlDatabase）的连接数、获取/释放时机、线程亲和性（是否每个线程独占连接）
- [ ] 连接断开后的重连策略（自动重连？报错回调？）
- [ ] MySQL 驱动（QSqlDriver）对象的创建与销毁时机，是否存在连接未 close 的资源泄漏
- [ ] `DBConfig` 中 `poolSize` 与 DBService 实例数的关系（main.cpp 按 `config.poolSize` 创建 N 个 DBService 实例）

### 3.5 任务取消与队列管理

- [ ] **已入队但尚未执行的任务**：服务端关闭时如何丢弃？（决定 `DBBridgeServer` 断线后孤儿任务的处理策略）
- [ ] 是否支持按 taskId 取消单个任务？（影响"客户端断开后取消 SQL"的可行性评估）
- [ ] 队列有界还是无界？客户端大量发请求时队列是否无限增长？

### 3.6 异常路径资源清理

- [ ] SQL 执行过程中发生 C++ 异常时的清理（数据库连接、语句句柄、内存）
- [ ] 日志记录策略：DBService 内部错误日志走什么通道？（`DBLogManager`？）日志是否可能无限增长
- [ ] 与 `DBLogManager` 的依赖关系及线程安全

## 4. 确认结果记录

> 确认依据：基于 `utils/DBService/src/` 完整源码（v1.4.1）逐项核实。

### 3.1 工作线程模型

| 问题编号 | 结论 | 影响 |
|---------|------|------|
| 3.1.1 | 使用 `QThread`，非 QtConcurrent。线程在 `init()` 中创建（`m_workerThread = new QThread(this)`），**固定 1 个工作线程/实例**；`start()` 仅启动调度定时器 | 每实例独占一个工作线程，线程数固定 |
| 3.1.2 | 有 FIFO 任务队列（`DBTaskManager::m_taskQueue`）。`onExecSqlList` 构造 DBTask 后经 `invokeMethod(..., Qt::QueuedConnection)` 入队，**串行执行，不立即执行** | 任务按入队顺序排队执行 |
| 3.1.3 | `TaskThreadModel` 仅两值：`MainThread`（主线程跑 SQL）/ `WorkerThread`（TaskManager moveToThread 到独立线程）；文档所述"其他模型"不存在 | 无额外模型可切换 |
| 3.1.4 | `sigTaskComplete` 连接为 AutoConnection，因跨线程实际为 **QueuedConnection**；投递亦显式 QueuedConnection | 跨线程信号异步安全 |

### 3.2 SQL 执行与回调保证

| 问题编号 | 结论 | 影响 |
|---------|------|------|
| 3.2.1 | **无任何 SQL 执行超时**（无 `setQueryTimeout`）。SQL 挂起（连接卡死/锁等待）时 `exec()` 阻塞在工作线程，`sigExecFinished` **永不回调** | `m_pendingCount` 永久挂起；阻塞线程退出（`wait(3000)` 超时 → 崩溃风险） |
| 3.2.2 | 正常失败（exec 返回 false）：`isSuccess=false`，`errCode=EXECUTE_FAILED/TRANS_ERR/DB_NOT_OPEN`，errMsg/errSql/errUnitIndex 均填充，**一定回调**（C++ 异常有 catch(...) 兜底）。**注意**：`SQL_SYNTAX_ERR` 从未被任何代码赋值 | 语法错误实际按 `EXECUTE_FAILED` 处理，走有限重试（最多 maxRetryCount 次），最终仍会回调（仅浪费重试） |
| 3.2.3 | **不保证一对一回调**：①队列满 REJECT 时 `pushTask` 返回 false，任务未入队，无回调；②`flushBatchBuffer` 队列满时静默丢弃，无回调 | 被拒/丢弃任务无 `sigExecFinished` → `m_pendingCount` 永久挂起 |
| 3.2.4 | `sigRawResultFinished` **不存在**。v1.4.0 已合并信号，唯一出口 `sigExecFinished`（携带 `rawJson`，QJsonDocument 隐式共享，零转换），按需 `extractData()` | 大批量数据在 `rawJson` 中随信号传递，生命周期由 Qt 隐式共享管理 |

### 3.3 启动/停止与线程退出

| 问题编号 | 结论 | 影响 |
|---------|------|------|
| 3.3.1 | `start()` 返回 void，无失败反馈；失败仅在 `init()` 时 qCritical 日志 | 调用方无法感知启动失败 |
| 3.3.2 | **DBService 无公开 `stop()`**。仅析构/`init()` 清理时执行 `quit() + wait(3000)` | 退出等待最多 3 秒 |
| 3.3.3 | **等待完成**（最多 3 秒），无强制中断；QSqlQuery 无法取消 | 挂起 SQL 会阻塞退出流程 |
| 3.3.4 | `wait(3000)` 超时后**无强制退出**：继续析构 → "QThread: Destroyed while thread is still running" 崩溃；或线程对象泄漏 | 进程能以崩溃方式结束（非优雅），不能保证软重启及时优雅退出 |

### 3.4 连接池与驱动资源

| 问题编号 | 结论 | 影响 |
|---------|------|------|
| 3.4.1 | **每实例 1 条连接**（1 个 QDBConnection/QSqlDatabase），非连接池。WorkerThread 模式下 QDBConnection 随 TaskManager 移入工作线程，由该线程独占使用 | N 个 DBService 实例 = N 条独立连接 |
| 3.4.2 | 自动重连：每次 `execTask` 前 `SELECT 1` ping，失败即 `doConnect()`；可配定时重试（enableRetry/retryTimes/retryIntervalMs），连接状态变化发 `sigConnectionChanged` | 断线自动恢复 |
| 3.4.3 | `doConnect()` 每次用新连接名 + 先置空再 `removeDatabase()`；QSqlQuery 均局部作用域且各路径调 `finish()` | 当前无泄漏。风险点：`removeDatabase()` 时存在存活查询则句柄泄漏（见"内存泄漏潜在风险"P1-4） |
| 3.4.4 | **`DBConfig` 无 `poolSize` 字段**（[dbconfig.h L36-49](file:///e:/dev/Project/vibeCoding/utils/DBService/src/DbAccess/DbAccessStruct/dbconfig.h#L36-L49)）。`poolSize` 为 server 端自己的配置 | N 个实例 = N 条独立连接，与 DBService 内部配置无关 |

### 3.5 任务取消与队列管理

| 问题编号 | 结论 | 影响 |
|---------|------|------|
| 3.5.1 | **无显式丢弃逻辑**：已入队未执行任务随 TaskManager 销毁静默消失，**不执行也不回调** | 服务端关闭时 `m_pendingCount` 挂起；若 DBService 仅重置不销毁，`m_pendingTasks` 条目残留泄漏 |
| 3.5.2 | **不支持按 taskId 取消**，无任何取消接口 | "客户端断开后取消 SQL"不可行，需改 DLL 接口（对应第 5 节问题4） |
| 3.5.3 | 任务队列**有界**（`queueMaxSize`：Standard/Reliable/Unreliable=100，HighPerf=500），批量缓冲有界。**BLOCK 策略未实现**——`pushTask` 两个分支均 `return false`（[L117-121](file:///e:/dev/Project/vibeCoding/utils/DBService/src/DbAccess/DBTaskManager/dbtaskmanager.cpp#L117-L121)） | 队列不无限增长；但 Reliable 配置的 BLOCK 实际行为是 REJECT（拒绝任务） |

### 3.6 异常路径资源清理

| 问题编号 | 结论 | 影响 |
|---------|------|------|
| 3.6.1 | `execTask`/`processTask` 均有 QException + catch(...) 兜底：回滚事务、构造失败结果回调；QSqlQuery 为栈对象自动析构；内存均为 Qt 值类型管理 | 异常路径清理干净 |
| 3.6.2 | 两套日志：qDebug/qWarning 控制台（Release 下编译为空操作）+ `DBLogManager` 异步写文件（按 serviceName+日期滚动，JSON Lines）。`m_pendingResults` **无条数上限** | 高负载下日志队列可能无限增长（见"内存泄漏潜在风险"P1-3） |
| 3.6.3 | 独立可选组件，经 `sigRequestRecord` QueuedConnection 提交。`onRequestRecord` 实际跑主线程（moveToThread 被注释），`flushToFile` 跑工作线程（DirectConnection），互斥锁仅保护 `m_pendingResults` | `m_logDir` 等成员跨线程访问存在数据竞争（见"内存泄漏潜在风险"P1-3） |

### 确认人

| 确认人 | 日期 |
|--------|------|
| AI 源码审查（基于 utils/DBService/src v1.4.1 源码） | 2026-07-31 |

## 5. 关联待做项

以下问题依赖 DBService 分析结果或属于其他模块，已登记待后续处理：

| 编号 | 问题 | 归属 | 状态 |
|------|------|------|------|
| 问题1 | 大批量下载临时文件孤儿残留（Server 崩溃后 temp_download 残留 .dat，启动时未清理） | 磁盘泄漏，非内存 | 待做 |
| 问题4 | 客户端断开后 DBService 孤儿任务继续执行（白跑 + pending 虚高） | 需改 DBService DLL 接口（支持按 taskId 取消） | 待做（依赖 3.5.x 分析结论） |

> **备注**：原问题2（QtConcurrent 后台压缩任务生命周期跟踪）已确认不再需要——当前版本已无软重启，仅有关闭与重启，事件循环退出时后台任务自然终止，风险消除。
