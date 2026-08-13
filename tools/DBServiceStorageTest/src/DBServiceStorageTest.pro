#-------------------------------------------------
# DBServiceStorageTest — DBService 大查询内存泄漏检验工具
#-------------------------------------------------
TEMPLATE = app
TARGET = DBServiceStorageTest
QT = core gui widgets
CONFIG += c++11

# DBService 对外头文件（编译产物，用户拖入）
INCLUDEPATH += $$PWD/3rdparty/DBService/include

# 链接 DBService DLL（Debug/Release 分开）
CONFIG(debug, debug|release) {
    LIBS += -L$$PWD/3rdparty/DBService/lib/debug -lDBService
} else {
    LIBS += -L$$PWD/3rdparty/DBService/lib/release -lDBService
}

# 运行时拷贝 DBService.dll 到构建输出目录
CONFIG(debug, debug|release) {
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$PWD/3rdparty/DBService/lib/debug/DBService.dll) $$shell_path($$OUT_PWD/debug/) $$escape_expand(\\n\\t)
} else {
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$PWD/3rdparty/DBService/lib/release/DBService.dll) $$shell_path($$OUT_PWD/release/) $$escape_expand(\\n\\t)
}

# Windows 进程内存读取（GetProcessMemoryInfo）
win32: LIBS += -lpsapi

# 日志时间戳：Debug 与 Release 均启用（%{time} 格式化开销极小，不影响压测结果）
DEFINES += DBSTORAGETEST_LOG_TIMESTAMP

SOURCES += \
    main.cpp \
    MainWindow.cpp

HEADERS += \
    MainWindow.h

FORMS += \
    MainWindow.ui
