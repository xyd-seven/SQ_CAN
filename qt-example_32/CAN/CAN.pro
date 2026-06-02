#-------------------------------------------------
#
# Project created by QtCreator 2022-08-02T14:43:03
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = CAN
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    canthread.cpp \
    imuparser.cpp \
    imuwidgets.cpp

HEADERS += \
        mainwindow.h \
    canthread.h \
    imuparser.h \
    imuwidgets.h

FORMS += \
        mainwindow.ui

RESOURCES += \
    resources.qrc

win32: RC_ICONS = icons/app.ico

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#dll¿âÒýÓÃ£¿ 1024²Â
win32: LIBS += -L$$PWD/./ -l zlgcan

INCLUDEPATH += $$PWD/.
DEPENDPATH += $$PWD/.

win32:!win32-g++: PRE_TARGETDEPS += $$PWD/./zlgcan.lib
#else:win32-g++: PRE_TARGETDEPS += $$PWD/./libControlCANFD.a

# 强制编译器以 UTF-8 处理源文件和执行字符集，彻底解决中文字符乱码
win32-g++ {
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}
win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
