#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "DBService.h"          // DBService 对外头文件（3rdparty/DBService/include）
#include "DBServiceStruct.h"    // DBServiceResult / SqlTuple / DBServiceType
#include "DBConfig.h"           // DBConfig / DBDriverType

namespace Ui {
class MainWindow;
}

/**
 * @brief DBService 大查询内存泄漏检验主窗口
 *
 * 用法：
 * 1. 填写数据库连接配置
 * 2. 在 SQL 输入框编写查询语句（大结果集 SELECT）
 * 3. 点击 [发送请求] → DBService 异步执行 → sigExecFinished 回调
 * 4. 观察：统计计数（总/成功/失败）、进程内存实时曲线
 *
 * 内存检验要点：回调槽中只计数、不保存结果，
 * 以验证"调用方不持有"前提下 DBService 本身是否存在内存泄漏。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    /// 发送请求：将 SQL 输入框内容投递给 DBService
    void onBtnSendClicked();
    /// 重新初始化：修改连接配置后重建 DBService
    void onBtnReinitClicked();
    /// 清理堆缓存：调用 _heapmin/HeapCompact 尝试归还空闲页，验证回收效果
    void onBtnHeapMinClicked();
    /// 自动执行按钮：点击开始（再次点击停止）按间隔自动执行 SQL
    void onBtnAutoClicked();
    /// 自动执行定时器触发：执行一次并调度下一次
    void onAutoTimeout();
    /// DBService 结果回调（只统计，不保存）
    void onTaskFinished(const DBServiceResult& result);

private:
    /// 根据界面配置创建（或重建）DBService
    void initDBService();
    /// 执行当前 SQL 框中的语句（手动/自动共用），返回 SQL 是否非空且已投递
    bool sendCurrentSql();
    /// 读取进程 Private Bytes（MB）
    double getMemoryMb() const;
    /// 刷新统计 label
    void updateStatsLabel();
    /// 刷新进程内存 label（每秒定时），变化超过阈值时写入记录区
    void updateMemoryLabel();
    /// 追加一行内存变化记录（限制总行数）
    void appendMemoryLog(const QString& line);

private:
    Ui::MainWindow *ui;

    DBService*  m_dbService = nullptr;   ///< DBService 实例（懒初始化）
    QTimer*     m_memTimer = nullptr;    ///< 内存刷新定时器
    QTimer*     m_autoTimer = nullptr;   ///< 自动执行定时器（单次触发）
    int         m_autoRemaining = 0;     ///< 自动执行剩余次数

    quint64     m_total = 0;             ///< 已发送请求总数
    quint64     m_success = 0;           ///< 成功回调数
    quint64     m_fail = 0;              ///< 失败回调数

    double      m_lastMemoryMb = 0.0;    ///< 上一次内存采样值（用于计算增量）
    bool        m_hasLastMemory = false; ///< 是否已有上一次采样值
};

#endif // MAINWINDOW_H
