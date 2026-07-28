# DBService v1.3.0 更新说明

> 版本：v1.3.0 | 日期：2026-07-27
> 本版本在 v1.2（线程模式选择）基础上，新增自定义 taskId 投递和原始 JSON 信号。

---

## 更新类型

- [ ] 修复（bug fix）
- [x] 功能（feature）
- [ ] 重构（refactor）
- [x] 文档（docs）
- [ ] 破坏性变更（breaking）

## 变更内容

### 新增

1. **`onExecSqlList(sqlList, taskId)` 重载**
   - 支持调用方传入自定义 taskId（如 IPC requestId）
   - `DBServiceResult` 原样带回，实现请求-结果精确匹配
   - 原 `onExecSqlList(sqlList)` 不变，零影响

2. **`DBServiceRawResult` 结构体**
   - 字段：`taskId / resultJson (QJsonObject) / isSuccess / errCode / errMsg / errSql`
   - 直传原始 JSON，跳过 `extractData` 的 Qt 类型转换
   - 已注册 `Q_DECLARE_METATYPE` + `qRegisterMetaType`

3. **`sigRawResult(const DBServiceRawResult&)` 信号**
   - 在 `sigExecFinished` 之后同步发射
   - DBBridgeServer 等框架层可直接获取原始 QJsonObject，零转换、零数据丢失
   - `sigExecFinished` 保留不删，两路信号并存

4. **对外头文件新增 `#include <QJsonObject>`**
   - `include/DBServiceStruct.h` 新增依赖

### 修复

5. **`QMetaObject::invokeMethod` 找不到方法**
   - WorkerThread 模式下，`start()` 和 `onPushTask()` 通过字符串名称的 `invokeMethod` 调用失败
   - 修复：两方法增加 `Q_INVOKABLE` 声明，注册到元对象系统

### 变更

6. 三个构造函数各增加 `qRegisterMetaType<DBServiceRawResult>("DBServiceRawResult")`

### 移除

无

## 兼容性说明

- [x] **完全向后兼容** — 所有改动均为新增接口/信号/结构体，无任何签名变更或移除
- [x] 业务项目直接替换 `.dll` + `.h` 即可，零代码改动

## 影响范围

| 文件 | 改动类型 |
|------|---------|
| `include/DBService.h` | 新增 `sigRawResult` 信号、`onExecSqlList(sqlList, taskId)` 槽 |
| `include/DBServiceStruct.h` | 新增 `DBServiceRawResult` 结构体、`#include <QJsonObject>` |
| `src/DBService/dbservice.h` | 同上（内部同步） |
| `src/DBService/dbservicestruct.h` | 同上（内部同步） |
| `src/DBService/dbservice.cpp` | 新增重载实现、`qRegisterMetaType`、`emit sigRawResult` |
| `src/DbAccess/DBTaskManager/dbtaskmanager.h` | 两处 `Q_INVOKABLE` |

## 文档

- `快速开始.md` — 新增"自定义 taskId"和"原始 JSON 信号"章节
- `build.md` — 更新说明表追加 5 条特性
- `设计文档.md` — 新增 3.3 原始 JSON 信号流程、4.8 设计决策
