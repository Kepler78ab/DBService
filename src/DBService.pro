# ============================================
# DBService v1.2 DLL 构建工程（支持线程模式选择）
# ============================================

TEMPLATE = lib
TARGET = DBService
CONFIG += dll

QT = core sql
CONFIG += c++11

# 版本号
VERSION_MAJOR = 1
VERSION_MINOR = 3
VERSION_PATCH = 0
DEFINES += DBSERVICE_VERSION=\"$$join(VERSION_MAJOR,.).$$VERSION_MINOR.$$VERSION_PATCH\"

# 导出宏（控制 Q_DECL_EXPORT）
DEFINES += DBSERVICE_LIBRARY

# 包含内部源码路径
INCLUDEPATH += \
    $$PWD \
    $$PWD/DBService \
    $$PWD/DbAccess/DbAccessStruct \
    $$PWD/DbAccess/QDBConnection \
    $$PWD/DbAccess/DBTaskManager

# 对外头文件安装路径（编译时供其它项目引用）
INCLUDEPATH += $$PWD/../include

# 引用内部源码模块
include($$PWD/DBService/DBService.pri)

# 编译输出目录
CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/../lib/debug
} else {
    DESTDIR = $$PWD/../lib/release
}
