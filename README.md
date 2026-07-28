# DBService

> **当前版本：v1.3.0**
>
> 基于 Qt 的轻量级数据库服务模块，封装为动态链接库（DLL）。
> 支持多服务类型、可配置重试策略、任务批处理、子线程模式、原始 JSON 直传。

---

## 快速导航

| 用途 | 文档 |
|------|------|
| 快速上手 | [快速开始.md](docs/快速开始.md) |
| 编译部署 | [build.md](docs/build.md) |
| 设计文档 | [docs/progress_docs/设计文档.md](docs/progress_docs/设计文档.md) |
| 更新日志 | [docs/updates_docs/update_v1.3.0.md](docs/updates_docs/update_v1.3.0.md) |
| 更新规范 | [docs/updates_docs/更新说明规范.md](docs/updates_docs/更新说明规范.md) |
| 子线程改造 | [docs/progress_docs/子线程DBService改造.md](docs/progress_docs/子线程DBService改造.md) |

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.3.0 | 2026-07-27 | 自定义 taskId、原始 JSON 信号 `sigRawResult`、Q_INVOKABLE 修复 |
| v1.2.0 | — | 线程模式选择（MainThread / WorkerThread）、Debug/Release 分离 |
| v1.1.0 | 2026-07-24 | 首次 DLL 化基准版本 |

## 目录结构

```
DBService/
├── include/              ← 对外头文件（业务项目引用）
├── src/                  ← 源码
│   ├── DBService.pro     ← DLL 构建工程
│   ├── DBService/        ← DBService 核心模块
│   └── DbAccess/         ← 内部数据库访问层
├── build/                ← 编译输出（Debug/Release 分别）
├── docs/                 ← 文档
│   ├── config/           ← 默认配置文件
│   ├── progress_docs/    ← 设计文档、改造记录
│   ├── updates_docs/     ← 版本更新说明
│   ├── 快速开始.md
│   └── build.md
├── bin/                  ← 默认配置模板
└── README.md
```
