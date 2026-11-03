QT += quick qml widgets

CONFIG += c++17 link_pkgconfig
PKGCONFIG += opencv4

SOURCES += \
    main.cpp \
    VideoProcessor.cpp \
    WhiteBalance.cpp

HEADERS += \
    VideoProcessor.h \
    WhiteBalance.h

RESOURCES += qml.qrc

# Отдельная папка для объектных файлов
OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
