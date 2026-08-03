# DBService v1.4.2 优化记录

> 版本：1.4.2 | 日期：2026-08-03
> 定位：大查询场景的 CPU 占用优化（内存优化见 v1.4.1）

## 变更内容

### 1. executeSqlUnit 列名预提取（降低逐行解析 CPU）

- **改动文件**：`src/DbAccess/QDBConnection/qdbconnection.cpp`
- **改动内容**：`record.fieldName(i)` 从"循环内每行每列调用"移至"查询前预提取到 `QStringList fieldNames`"，循环内复用（`fieldNames.at(i)`）
- **收益**：90970 行 × 25 列场景下，消除约 227 万次 `fieldName()` 内部查找与 QString 构造
- **影响**：纯内部优化，接口不变，结果格式不变

### 2. 测试工具移除 rawJson 深拷贝统计

- **改动文件**：`DBServiceStorageTest/src/MainWindow.cpp`
- **改动内容**：`onTaskFinished` 不再调用 `result.rawJson.object()`（该调用会深拷贝整个结果集，大查询时每次回调额外克隆约 160MB），行数改由 DBService 日志打印
- **收益**：消除调用方侧每次回调的一次全量深拷贝（CPU + 内存双降）
- **影响**：仅测试工具；生产调用方如有类似"仅统计不取数"的路径，同样应避免 `object()`

### 3. Worker 线程低优先级（大查询不抢占 CPU）

- **改动文件**：`src/DBService/dbservice.cpp`
- **改动内容**：`m_workerThread->start(QThread::LowPriority)`，WorkerThread 模式下 DB 执行/解析线程以低优先级调度
- **收益**：大查询期间（单核 100% 持续数秒）不再抢占主线程与其他进程的 CPU，进程整体响应性提升
- **影响**：执行耗时几乎不变（优先级只影响调度，不影响计算量）

## 验证方法

1. 重编译 DBService + 测试工具，拖入 3rdparty dll；
2. 0.5s × 50 次大查询压测，观察：回调日志 rows 正常、内存曲线无明显变化；
3. 观察任务管理器：查询期间该进程 CPU 占用不变但**系统其他任务响应性提升**。

## 待评估项（未实施）

### 紧凑数据模型（可选，接口变更）

- **现状**：每行 `QJsonObject`（重复存 25 个键名 + 哈希节点）
- **方案**：结果格式改为 `{"tag": {"columns": [...], "rows": [[...], ...]}}`，列头存一次、每行只存值
- **收益**：内存约减 60%（160MB → ~60MB）、CPU 显著下降（消除 227 万次哈希插入）
- **代价**：`rawJson` 内部格式变化；`extractData` 需兼容转换 + 新增 `extractColumnsAndRows` 高效接口
- **状态**：待确认后实施
