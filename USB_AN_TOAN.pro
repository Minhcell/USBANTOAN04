QT += core gui widgets

CONFIG += c++11 release static
TARGET = USB_AN_TOAN
TEMPLATE = app

# Plugin nen tang Windows cho ban static
QTPLUGIN += qwindows

SOURCES += \
    main.cpp \
    diskio.cpp \
    sectorfs.cpp \
    diskutil.cpp \
    workers.cpp \
    logindialog.cpp \
    setupwindow.cpp \
    mainwindow.cpp

HEADERS += \
    common.h \
    diskio.h \
    sectorfs.h \
    diskutil.h \
    workers.h \
    logindialog.h \
    setupwindow.h \
    mainwindow.h

# Manifest yeu cau quyen Admin (runner can doc/ghi sector)
RC_FILE = app.rc

LIBS += -lshell32 -ladvapi32 -lole32 -luuid

# Link tinh hoan toan -> 1 file EXE duy nhat, khong can DLL
QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++

win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
    DEFINES += UNICODE _UNICODE
}
