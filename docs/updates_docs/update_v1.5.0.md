# DBService v1.5.0 变更设计：紧凑数据模型

> 状态：**实现中（代码完成，待编译验证）** | 日期：2026-08-03
> 定位：查询/写操作结果格式全面切换为紧凑模型，降低大查询 CPU 与内存占用
> 前置：v1.4.1（内存优化）+ v1.4.2（CPU 优化）已落地

## 一、新格式设计

每 tuple 的结果从"行对象数组"改为"类型标记 + 元数据"：

```json
// 查询类（isModify=false）
{
  "tag": {
    "type": "query",
    "columns": ["id", "name", "value"],
    "rows": [
      [1, "AAA", 123.45],
      [2, "BBB", 678.90]
    ]
  }
}

// 写类（isModify=true）
{
  "tag": {
    "type": "write",
    "affectedRows": 3,
    "lastInsertId": 10        // 可选：仅 INSERT 且驱动支持时出现
  }
}
```

- **columns 只存一次**（不再每行重复 25 份键名）；
- **rows 每行是纯值 QJsonArray**（无键名、无哈希节点）；
- **type 字段**显式区分查询/写，避免调用方猜测键。

## 二、四种场景完整 DBServiceResult 前后对比

> 公共字段（isSuccess / serviceName / taskId / sqlList / errCode / errTupleIndex / data）行为不变，仅 rawJson 内部格式变化。

### 场景 1：单 tuple 纯查询

`execSql(taskId, "hist", "SELECT * FROM histtest", false)`

**旧（v1.4.x）rawJson**：
```json
{ "hist": [ {"id":1,"name":"AAA"}, {"id":2,"name":"BBB"} ] }
```

**新（v1.5.0）rawJson**：
```json
{ "hist": { "type":"query", "columns":["id","name"], "rows":[[1,"AAA"],[2,"BBB"]] } }
```

**完整 DBServiceResult（新旧一致的部分）**：
```cpp
DBServiceResult r;
r.isSuccess     = true;
r.serviceName   = "db1";
r.taskId        = "5500d263-xxxx";
r.sqlList       = { SqlTuple("hist", "SELECT * FROM histtest", false) };
r.errCode       = DBErrCode::SUCCESS;
r.errTupleIndex = -1;
r.data          = {};   // 默认空，按需 extractData
// r.rawJson 见上
```

### 场景 2：多 tuple 纯查询（2 条 SELECT）

**旧**：
```json
{
  "t1": [ {"id":1}, {"id":2} ],
  "t2": [ {"id":10}, {"id":20} ]
}
```

**新**：
```json
{
  "t1": { "type":"query", "columns":["id"], "rows":[[1],[2]] },
  "t2": { "type":"query", "columns":["id"], "rows":[[10],[20]] }
}
```

### 场景 3：查写并存（1 SELECT + 1 INSERT）

**旧**：
```json
{
  "sel": [ {"id":1, "name":"AAA"} ],
  "ins": [ {"affectedRows":1, "lastInsertId":100} ]
}
```

**新**：
```json
{
  "sel": { "type":"query", "columns":["id","name"], "rows":[[1,"AAA"]] },
  "ins": { "type":"write", "affectedRows":1, "lastInsertId":100 }
}
```

### 场景 4：纯写（2 条写 SQL）

**旧**：
```json
{
  "upd": [ {"affectedRows":3} ],
  "ins": [ {"affectedRows":1, "lastInsertId":10} ]
}
```

**新**：
```json
{
  "upd": { "type":"write", "affectedRows":3 },
  "ins": { "type":"write", "affectedRows":1, "lastInsertId":10 }
}
```

## 三、数据对比（90970 行 × 25 列基准）

| 指标 | 旧格式（对象数组） | 新格式（紧凑） | 降幅 |
|------|--------------------|----------------|------|
| 键名存储 | 90970 × 25 份 | 25 份（仅 columns） | — |
| 哈希插入 | 227 万次（每格一次） | **0 次** | — |
| 内存（结果树） | ~160MB | ~60MB（估） | **~60%** |
| DB 侧 CPU（构建） | 高 | 显著下降 | — |
| 调用方解析（toObject 深拷贝） | 227 万次节点拷贝 | rows 一次提取 + 90970 次纯值拷贝 | — |
| QJsonDocument 深拷贝（execTask） | 整棵对象树 | 整棵紧凑树（更小） | 同比例降 |

## 四、改造方案

1. **executeSqlUnit**（`qdbconnection.cpp`）：改为直接输出紧凑结构
   - 查询分支：`{"type":"query", "columns":[...], "rows":[...]}`（columns 用 v1.4.2 已预提取的 fieldNames，零额外成本）
   - 写分支：`{"type":"write", "affectedRows":n[, "lastInsertId":x]}`
2. **execTask**（`qdbconnection.cpp`）：`resultObj[unit.jsonKey] = 紧凑对象`（QJsonValue 共享，O(1)），多单元组装逻辑不变
3. **extractData 全面转向新格式**（`DBService.h/.cpp`）：
   - 保留签名 `QMap<QString, QVariant> extractData(const QJsonDocument&)`，但**按新格式解析、输出新结构**（不再转旧行对象数组）
   - 查询 tag → `QVariantMap{ "columns": QStringList, "rows": QJsonArray }`（紧凑，零转换）
   - 写 tag → `QVariantMap{ "affectedRows": qint64, "lastInsertId": qint64 }`
   - 删除旧格式兼容转换逻辑
4. **新增便捷接口**：
   - `bool extractColumnsAndRows(const QJsonDocument& doc, const QString& tag, QStringList* columns, QJsonArray* rows)` — 查询结果零转换
   - `bool extractWriteResult(const QJsonDocument& doc, const QString& tag, qint64* affectedRows, qint64* lastInsertId)` — 写结果
5. **DBServiceStruct.h**：rawJson 格式注释更新
6. **测试工具**：行数统计改用 `extractColumnsAndRows`（仍零拷贝原则）

## 五、任务清单

- [x] 1. `executeSqlUnit` 查询/写分支输出紧凑结构
- [x] 2. `execTask` 组装适配（resultObj 值类型变化，单元/异常路径复核）
- [x] 3. `extractData` 重定义为新格式解析（查询→{columns,rows}、写→{affectedRows,lastInsertId}），删除旧兼容逻辑
- [x] 4. 新增 `extractColumnsAndRows` / `extractWriteResult`
- [x] 5. `DBServiceStruct.h` / `DBService.h` 注释与接口文档更新
- [x] 6. 测试工具适配（行数统计改高效接口）
- [ ] 7. 编译验证（DBService + 测试工具）
- [ ] 8. 压测验证：0.5s × 50 大查询，对比 v1.4.x 的峰值内存与 CPU 占用
- [ ] 9. 更新 update_v1.5.0.md 实际结果

## 六、兼容性说明

> **本次为破坏性变更**：rawJson 格式与 extractData 输出结构均全面转向新格式，不做旧格式兼容转换。

| 项 | 说明 |
|----|------|
| DBService 接口（函数签名） | 不变（execSql / 信号 / extractData 签名保留） |
| **rawJson 格式** | 破坏性变化：旧行对象数组 → 新 `{type, columns, rows}` / `{type, affectedRows}` |
| **extractData 输出** | 破坏性变化：查询 → `{columns, rows}` 紧凑结构；写 → `{affectedRows, lastInsertId}` |
| 调用方适配 | 直接读 rawJson 的调用方按 `type` 字段解析；用 extractData 的调用方按新结构取值 |
| 数据（`QVector<QVariantMap>` 行对象） | 旧调用方如需行对象，需自行用 `columns+rows` 重建（DBService 不再代劳） |
| 多单元/事务/重试路径 | 逻辑不变，仅结果容器格式变化，需回归验证 |
