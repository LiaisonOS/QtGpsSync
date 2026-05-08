QT += core gui widgets serialport

TARGET = QtGpsSync
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050000

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    NmeaParser.cpp \
    GpsSyncer.cpp \
    GpsMonitor.cpp

HEADERS += \
    MainWindow.h \
    NmeaParser.h \
    GpsSyncer.h \
    GpsMonitor.h

# Install
target.path = /opt/emcomm-tools/bin/QtGpsSync
INSTALLS += target
