# DBService v1.5.1 更新说明

> 版本：v1.5.1 | 日期：2026-08-03
> 前置：v1.5.0（紧凑数据模型）已落地

---

## 更新类型

- [ ] 修复（bug fix）
- [x] 功能（feature）
- [ ] 重构（refactor）
- [ ] 文档（docs）
- [ ] 破坏性变更（breaking）

## 变更内容

### 新增

- **DBServicePool**：DBService 的拓展用法——持有 N 个 DBService 实例（WorkerThread 模式）的"无状态调度壳"，提供任务分发 + 信号聚合 + 配置克隆：
  - `execSql(taskId, sqlList)` / `execSql(taskId, tag, sql, isModify)`：自动负载均衡分发（最少任务数）；
  - `execSqlOn(instanceIndex, ...)`：定向分发，跳过负载均衡；
  - 状态查询：`poolSize()` / `pendingCount()` / `pendingCount(index)` / `serviceNames()`；
  - 信号 `sigExecFinished` 聚合转发（任一实例完成即触发，`serviceName` 可溯源）。
- **`DBService::pendingCount()`**：返回当前排队中的任务数（线程安全），供 Pool 负载均衡选实例。
- 公开头文件 `include/DBServicePool.h`（与 `DBService.h` 同属对外 API）。

### 修复

- 无

### 变更

- 版本号升至 `v1.5.1`（`DBService.pro` `VERSION_PATCH = 1`、`DBService::currentVersion()` 返回 `v1.5.1`）。
- **Debug/Release DLL 区分命名**：Debug → `DBServiced.dll`，Release → `DBService.dll`（避免混用）。
- **DLL 嵌入版本资源**：通过 `version_info.rc`（右键属性 → 详细信息 → 产品版本 `1.5.1`）。

### 移除

- 无

## 兼容性说明

- **DLL 向后兼容**：是。仅新增类与新增只读查询方法，未修改任何现有接口/行为。
- **业务项目是否需要修改代码**：否。原有 `DBService` 用法不变；如需并发可改用 `DBServicePool`（可选）。

## 影响范围

- `include/DBService.h` — 新增 `pendingCount()` 声明
- `include/DBServicePool.h` — 新增（对外 API）
- `src/DBService/dbservice.h` / `dbservice.cpp` — 新增 `pendingCount()`
- `src/DBServicePool/` — 新增 DBServicePool 模块（.h/.cpp/.pri）
- `src/DBService.pro` — 引入 DBServicePool 模块、版本号升至 1.5.1、Debug/Release 区分 TARGET、RC_FILE 版本资源
- `src/version_info.rc` — 新增，用于 DLL 版本资源

## 迁移指南

- **业务项目 .pro**：Debug 链接库名需从 `-lDBService` 改为 `-lDBServiced`（Release 不变）。
- **运行时 DLL**：Debug 模式需部署 `DBServiced.dll`，Release 模式部署 `DBService.dll`。

## 用法示例

```cpp
// 单实例用法（等同直接使用 DBService）
DBServicePool pool(cfg, 1);
connect(&pool, &DBServicePool::sigExecFinished, this, &MainWindow::onFinished);
pool.init();
pool.start();

// 并发用法：4 路并发，等价于手动声明 4 个 DBService，扩容只改 poolSize
DBServicePool pool4(cfg, 4);
pool4.init();
pool4.start();
pool4.execSql(taskId, "hist", "SELECT * FROM histtest", false);   // 自动分发到最空闲实例
```
