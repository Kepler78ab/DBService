# DBService

> **当前版本：v1.5.1**
>
> 基于 Qt 的轻量级数据库服务模块，封装为动态链接库（DLL）。
> 支持多服务类型、可配置重试策略、任务批处理、子线程模式、原始 JSON 直传。
>
> v1.5.1 新增 DBServicePool：DBService 的拓展用法——N 实例负载均衡分发 + 信号聚合，
> 单实例/并发可自由选择，扩容只需改 poolSize。

---

## 快速导航

| 用途 | 文档 |
|------|------|
| 快速上手 | [docs/快速开始.md](DBService/docs/快速开始.md) |
| 编译部署 | [docs/build.md](DBService/docs/build.md) |
| 设计文档 | [docs/progress_docs/设计文档.md](DBService/docs/progress_docs/设计文档.md) |
| DBServicePool 设计 | [docs/progress_docs/DBServicePool设计.md](DBService/docs/progress_docs/DBServicePool设计.md) |
| 内存优化方案 | [docs/progress_docs/内存优化.md](DBService/docs/progress_docs/内存优化.md) |
| 更新日志 v1.5.1 | [docs/updates_docs/update_v1.5.1.md](DBService/docs/updates_docs/update_v1.5.1.md) |
| 更新日志 v1.5.0 | [docs/updates_docs/update_v1.5.0.md](DBService/docs/updates_docs/update_v1.5.0.md) |
| 更新日志 v1.4.1 | [docs/updates_docs/update_v1.4.1.md](DBService/docs/updates_docs/update_v1.4.1.md) |
| 更新日志 v1.4.0 | [docs/updates_docs/update_v1.4.0.md](DBService/docs/updates_docs/update_v1.4.0.md) |
| 更新日志 v1.3.0 | [docs/updates_docs/update_v1.3.0.md](DBService/docs/updates_docs/update_v1.3.0.md) |
| 更新规范 | [docs/updates_docs/更新说明规范.md](DBService/docs/updates_docs/更新说明规范.md) |

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.5.1 | 2026-08-03 | 新增 DBServicePool（N 实例负载均衡分发 + 信号聚合）、Debug/Release DLL 区分命名、DLL 版本资源 |
| v1.5.0 | 2026-08-03 | 紧凑数据模型（columns 单存 + type 标记），rawJson 格式全面切换，性能/内存大幅下降 |
| v1.4.1 | 2026-07-28 | BatchBuffer 共享指针、std::move 消除剩余深拷贝、Release 消除日志开销 |
| v1.4.0 | 2026-07-28 | 合并信号、内存优化、extractData 按需调用、TaskNode 共享指针 |
| v1.3.0 | 2026-07-27 | 自定义 taskId、原始 JSON 信号 `sigRawResult`、Q_INVOKABLE 修复 |
| v1.2.0 | — | 线程模式选择（MainThread / WorkerThread）、Debug/Release 分离 |
| v1.1.0 | 2026-07-24 | 首次 DLL 化基准版本 |

## 目录结构

```
DBService/
├── include/              ← 对外头文件（业务项目引用）
├── src/                  ← 源码
│   ├── DBService.pro     ← DLL 构建工程
│   ├── version_info.rc   ← DLL 版本资源（v1.5.1）
│   ├── DBService/        ← DBService 核心模块
│   ├── DBServicePool/    ← DBServicePool 拓展模块（v1.5.1）
│   └── DbAccess/         ← 内部数据库访问层
├── lib/                  ← 编译输出
│   ├── release/
│   │   ├── DBService.dll     ← Release DLL
│   │   └── libDBService.a
│   └── debug/
│       ├── DBServiced.dll    ← Debug DLL（d 后缀）
│       └── libDBServiced.a
├── docs/                 ← 文档
│   ├── config/           ← 默认配置文件
│   ├── progress_docs/    ← 设计文档、改造记录
│   ├── updates_docs/     ← 版本更新说明
│   ├── 快速开始.md
│   └── build.md
├── bin/                  ← 默认配置模板
└── README.md
```
