#include <QApplication>
#include <QDebug>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#ifdef DBSTORAGETEST_LOG_TIMESTAMP
    // 时间戳：该 Qt 版本将 %{time [format]} 的方括号并入格式串，因此外层不再重复包裹
    qSetMessagePattern("%{time [hh:mm:ss.zzz]} %{message}");
#endif

    MainWindow w;
    w.show();
    return a.exec();
}
