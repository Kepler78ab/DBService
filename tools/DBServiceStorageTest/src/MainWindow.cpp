#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QDebug>
#include <QMessageBox>
#include <QTextCursor>
#include <QTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <malloc.h>   // _heapmin
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->btnSend, &QPushButton::clicked,
            this, &MainWindow::onBtnSendClicked);
    connect(ui->btnReinit, &QPushButton::clicked,
            this, &MainWindow::onBtnReinitClicked);
    connect(ui->btnHeapMin, &QPushButton::clicked,
            this, &MainWindow::onBtnHeapMinClicked);
    connect(ui->btnAuto, &QPushButton::clicked,
            this, &MainWindow::onBtnAutoClicked);

    // 每秒刷新进程内存显示
    m_memTimer = new QTimer(this);
    m_memTimer->setInterval(1000);
    connect(m_memTimer, &QTimer::timeout, this, &MainWindow::updateMemoryLabel);
    m_memTimer->start();
    updateMemoryLabel();

    // 自动执行定时器：单次触发，每次执行后按间隔重新启动
    m_autoTimer = new QTimer(this);
    m_autoTimer->setSingleShot(true);
    connect(m_autoTimer, &QTimer::timeout, this, &MainWindow::onAutoTimeout);

    updateStatsLabel();
}

MainWindow::~MainWindow()
{
    // DBService 有 parent(this)，随主窗口析构；先断开信号避免回调
    if (m_dbService) {
        disconnect(m_dbService, nullptr, this, nullptr);
    }
    delete ui;
}

void MainWindow::initDBService()
{
    // 重建：旧实例交由 Qt 析构（有 parent）
    if (m_dbService) {
        disconnect(m_dbService, nullptr, this, nullptr);
        m_dbService->deleteLater();
        m_dbService = nullptr;
    }

    DBConfig cfg;
    cfg.driverType     = DBDriverType::MySQL;          // 预留：如需切换驱动在此修改
    cfg.host           = ui->editHost->text().trimmed();
    cfg.port           = ui->editPort->text().trimmed().toUShort();
    cfg.dbName         = ui->editDbName->text().trimmed();
    cfg.user           = ui->editUser->text().trimmed();
    cfg.password       = ui->editPassword->text();

    // 预留：可切换服务类型 / 线程模式
    DBServiceType type = DBServiceType::Standard;
    TaskThreadModel model = TaskThreadModel::WorkerThread;

    m_dbService = new DBService(type, cfg, model, this);
    connect(m_dbService, &DBService::sigExecFinished,
            this, &MainWindow::onTaskFinished);

    m_dbService->start();
    ui->lblLastResult->setText(tr("DBService 已初始化 (type=Standard, model=WorkerThread)"));
}

void MainWindow::onBtnSendClicked()
{
    sendCurrentSql();
}

bool MainWindow::sendCurrentSql()
{
    const QString sql = ui->editSql->toPlainText().trimmed();
    if (sql.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先填写 SQL 语句"));
        return false;
    }

    if (!m_dbService) {
        initDBService();
    }

    // 预留：SQL 可拆分为多条 SqlTuple，此处按单条发送
    // 如需修改 tag / isModify，在此调整
    QVector<SqlTuple> sqlList;
    sqlList.append(SqlTuple("query", sql, /*isModify=*/false));
    m_dbService->onExecSqlList(sqlList);

    ++m_total;
    updateStatsLabel();
    ui->lblLastResult->setText(tr("已发送请求 #%1: %2")
                               .arg(m_total)
                               .arg(sql.left(60)));
    return true;
}

void MainWindow::onBtnAutoClicked()
{
    // 正在自动执行：点击则停止
    if (m_autoTimer->isActive() || m_autoRemaining > 0) {
        m_autoTimer->stop();
        m_autoRemaining = 0;
        ui->btnAuto->setText(tr("自动执行"));
        ui->spinInterval->setEnabled(true);
        ui->spinCount->setEnabled(true);
        ui->lblLastResult->setText(tr("已停止自动执行"));
        return;
    }

    // 校验 SQL
    if (ui->editSql->toPlainText().trimmed().isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先填写 SQL 语句"));
        return;
    }

    m_autoRemaining = ui->spinCount->value();
    ui->btnAuto->setText(tr("停止自动执行"));
    ui->spinInterval->setEnabled(false);
    ui->spinCount->setEnabled(false);
    ui->lblLastResult->setText(tr("自动执行开始：剩余 %1 次").arg(m_autoRemaining));

    // 立即执行第一次
    onAutoTimeout();
}

void MainWindow::onAutoTimeout()
{
    // 结束（次数归零或被手动停止）
    if (m_autoRemaining <= 0) {
        m_autoRemaining = 0;
        ui->btnAuto->setText(tr("自动执行"));
        ui->spinInterval->setEnabled(true);
        ui->spinCount->setEnabled(true);
        return;
    }

    // 执行一次；SQL 为空时提前结束
    if (!sendCurrentSql()) {
        m_autoRemaining = 0;
        ui->btnAuto->setText(tr("自动执行"));
        ui->spinInterval->setEnabled(true);
        ui->spinCount->setEnabled(true);
        return;
    }

    --m_autoRemaining;

    if (m_autoRemaining > 0) {
        // 调度下一次：间隔从"本次请求发出"起算（单次定时器）
        const int intervalMs = qRound(ui->spinInterval->value() * 1000.0);
        m_autoTimer->start(intervalMs);
    } else {
        // 全部执行完毕
        ui->btnAuto->setText(tr("自动执行"));
        ui->spinInterval->setEnabled(true);
        ui->spinCount->setEnabled(true);
        ui->lblLastResult->setText(tr("自动执行完成，共 %1 次").arg(ui->spinCount->value()));
    }
}

void MainWindow::onBtnReinitClicked()
{
    initDBService();
}

void MainWindow::onBtnHeapMinClicked()
{
    const double before = getMemoryMb();

    // 压缩 CRT 堆 + Windows 默认进程堆：合并空闲块并尝试归还尾部空闲页
#if defined(_MSC_VER) || defined(__MINGW32__)
    _heapmin();
#endif
    if (HANDLE h = GetProcessHeap()) {
        HeapCompact(h, 0);
    }

    const double after = getMemoryMb();
    m_lastMemoryMb = after;
    m_hasLastMemory = true;
    ui->lblMemory->setText(tr("进程内存(Private): %1 MB").arg(after, 0, 'f', 1));
    appendMemoryLog(tr("[%1] 堆清理: %2 MB → %3 MB (回收 %4 MB)")
                    .arg(QTime::currentTime().toString("HH:mm:ss"))
                    .arg(before, 0, 'f', 1)
                    .arg(after, 0, 'f', 1)
                    .arg(before - after, 0, 'f', 1));
}

void MainWindow::onTaskFinished(const DBServiceResult& result)
{
    // 只计数，不保存结果 —— 保证"调用方不持有"前提，检验 DBService 自身是否泄漏
    // 行数统计用零转换接口 extractColumnsAndRows（rows 赋值共享，无深拷贝）
    if (result.isSuccess) {
        ++m_success;
        QJsonArray rows;
        qint64 rowCount = -1;
        if (DBService::extractColumnsAndRows(result.rawJson,
                                             QStringLiteral("query"), nullptr, &rows)) {
            rowCount = rows.size();
        }
        ui->lblLastResult->setText(rowCount >= 0
            ? tr("成功 | taskId=%1 | 行数=%2").arg(result.taskId).arg(rowCount)
            : tr("成功 | taskId=%1 | 行数见日志").arg(result.taskId));
    } else {
        ++m_fail;
        ui->lblLastResult->setText(tr("失败 | taskId=%1 | errCode=%2 | %3")
                                   .arg(result.taskId)
                                   .arg(static_cast<int>(result.errCode))
                                   .arg(result.errMsg.left(80)));
    }
    updateStatsLabel();
}

void MainWindow::updateStatsLabel()
{
    ui->lblTotal->setText(tr("总请求: %1").arg(m_total));
    ui->lblSuccess->setText(tr("成功: %1").arg(m_success));
    ui->lblFail->setText(tr("失败: %1").arg(m_fail));
}

double MainWindow::getMemoryMb() const
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX pmc;
    memset(&pmc, 0, sizeof(pmc));
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

void MainWindow::updateMemoryLabel()
{
#ifdef Q_OS_WIN
    const double mb = getMemoryMb();
    ui->lblMemory->setText(tr("进程内存(Private): %1 MB").arg(mb, 0, 'f', 1));

    // 变化超过阈值时记录（时间 + 内存值 + 增量）
    if (m_hasLastMemory) {
        const double delta = mb - m_lastMemoryMb;
        if (qAbs(delta) >= 0.5) {
            appendMemoryLog(tr("[%1] %2 MB  (Δ %3 MB)")
                            .arg(QTime::currentTime().toString("HH:mm:ss"))
                            .arg(mb, 0, 'f', 1)
                            .arg(delta, 0, 'f', 1));
        }
    }
    m_lastMemoryMb = mb;
    m_hasLastMemory = true;
#endif
}

void MainWindow::appendMemoryLog(const QString& line)
{
    // 限制总行数，避免记录区无限增长
    const int maxLines = 500;
    if (ui->editMemLog->blockCount() > maxLines) {
        QTextCursor cursor = ui->editMemLog->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);  // 选中第一行
        cursor.removeSelectedText();
        cursor.deleteChar();  // 删除第一行末尾的换行符
    }
    ui->editMemLog->appendPlainText(line);
}
