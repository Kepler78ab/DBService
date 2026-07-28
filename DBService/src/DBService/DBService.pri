# DBService 模块
HEADERS += \
    $$PWD/dbservice.h \
    $$PWD/dbservicestruct.h \
    $$PWD/DBLogManager.h

SOURCES += \
    $$PWD/dbservice.cpp \
    $$PWD/DBLogManager.cpp

# 包含路径
INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

# 依赖 DbAccess 模块
include(../DbAccess/DbAccess.pri)