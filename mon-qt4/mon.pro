######################################################################
# Automatically generated by qmake (2.00a) Sat Jun 3 09:19:25 2006
######################################################################

TEMPLATE = app
TARGET += 
DEPENDPATH += .
QT += xml
LIBS += -L/opt/kde4/lib -lkdeui -lkdecore -lDCOP -lkdefx ../bgcc/services/.libs/libcommon.a
INCLUDEPATH += /opt/kde4/include . ..

# Input
HEADERS += detailedhostview.h \
           ganttstatusview.h \
           hostinfo.h \
           hostlistview.h \
           hostview.h \
           job.h \
           joblistview.h \
           listview.h \
           mon-kde.h \
           monitor.h \
           starview.h \
           statusview.h \
           summaryview.h
SOURCES += detailedhostview.cc \
           ganttstatusview.cc \
           hostinfo.cc \
           hostlistview.cc \
           hostview.cc \
           job.cc \
           joblistview.cc \
           listview.cc \
           mon-kde.cc \
           monitor.cc \
           starview.cc \
           statusview.cc \
           summaryview.cc
#The following line was inserted by qt3to4
QT +=  qt3support 
