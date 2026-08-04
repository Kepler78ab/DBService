# ============================================
# DBService v1.2 DLL 构建工程（支持线程模式选择）
# ============================================

TEMPLATE = lib
CONFIG += dll

# Debug/Release 区分 DLL 命名
CONFIG(debug, debug|release) {
    TARGET = DBServiced
} else {
    TARGET = DBService
}

QT = core sql
CONFIG += c++11

msvc {
    QMAKE_CXXFLAGS += /utf-8
}

# 版本号（与 dbservice.cpp currentVersion() 保持一致）
VERSION_MAJOR = 1
VERSION_MINOR = 5
VERSION_PATCH = 1
# 通过 .rc 文件嵌入版本资源（避免 qmake VERSION 变量导致 DLL 文件名带后缀）
RC_FILE = $$PWD/version_info.rc

# Release 编译时消除调试日志开销
CONFIG(release, debug|release) {
    DEFINES += QT_NO_DEBUG_OUTPUT QT_NO_WARNING_OUTPUT
}

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
include($$PWD/DBServicePool/DBServicePool.pri)

# 编译输出目录
CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/../lib/debug
} else {
    DESTDIR = $$PWD/../lib/release
}
