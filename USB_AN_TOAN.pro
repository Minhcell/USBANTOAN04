QT += core gui widgets

CONFIG += c++11 release static
TARGET = USB_AN_TOAN
TEMPLATE = app

# Ban static: qmake TU DONG import plugin nen tang qwindows.
# KHONG khai bao QTPLUGIN += qwindows va KHONG Q_IMPORT_PLUGIN thu cong
# (tranh import 2 lan -> loi trung ky hieu luc link).

SOURCES += \
    main.cpp \
    diskio.cpp \
    sectorfs.cpp \
    diskutil.cpp \
    workers.cpp \
    logindialog.cpp \
    setupwindow.cpp \
    mainwindow.cpp \
    crypto.cpp

HEADERS += \
    common.h \
    diskio.h \
    sectorfs.h \
    diskutil.h \
    workers.h \
    logindialog.h \
    setupwindow.h \
    mainwindow.h \
    crypto.h

# Manifest yeu cau quyen Admin (runner can doc/ghi sector)
RC_FILE = app.rc

# -lbcrypt: ma hoa AES-256 + PBKDF2 (Windows CNG, tang toc AES-NI)
LIBS += -lshell32 -ladvapi32 -lole32 -luuid -lbcrypt

# Link tinh hoan toan -> 1 file EXE duy nhat, khong can DLL
QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++

win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
    DEFINES += UNICODE _UNICODE
}
