HEADERS += \
    $$PWD/DbAccessStruct/dbconfig.h \
    $$PWD/DbAccessStruct/sqlunit.h \
    $$PWD/DbAccessStruct/dbtask.h \
    $$PWD/DbAccessStruct/dbtaskresult.h \
    $$PWD/DbAccessStruct/dbstructs.h \
    $$PWD/QDBConnection/qdbconnection.h \
    $$PWD/DBTaskManager/dbtaskmanager.h

SOURCES += \
    $$PWD/QDBConnection/qdbconnection.cpp \
    $$PWD/DBTaskManager/dbtaskmanager.cpp

INCLUDEPATH += $$PWD/DbAccessStruct \
               $$PWD/QDBConnection \
               $$PWD/DBTaskManager