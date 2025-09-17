QT += core widgets network

CONFIG += c++17

TARGET = DroneRadarSystem
TEMPLATE = app

# Qt 6.9 兼容性设置 - 禁用问题检查
DEFINES += QT_NO_COMPARE_HELPERS_ALL
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060900
DEFINES += QT_NO_DEPRECATED_WARNINGS
DEFINES += QT_DEPRECATED_WARNINGS_SINCE=0x000000
DEFINES += NDEBUG
QMAKE_CXXFLAGS += -Wno-deprecated-declarations
QMAKE_CXXFLAGS += -Wno-error=deprecated-declarations
QMAKE_CXXFLAGS += -fpermissive
QMAKE_CXXFLAGS += -Wno-error
QMAKE_CXXFLAGS += -w
QMAKE_CXXFLAGS += -DNDEBUG

# 包含路径
INCLUDEPATH += include

# 源文件
SOURCES += \
    src/main.cpp \
    src/Drone.cpp \
    src/DroneManager.cpp \
    src/RadarSimulator.cpp \
    src/RadarDisplay.cpp \
    src/StatisticsManager.cpp \
    src/WeaponStrategy.cpp

# 头文件
HEADERS += \
    include/Drone.h \
    include/DroneManager.h \
    include/RadarSimulator.h \
    include/RadarDisplay.h \
    include/StatisticsManager.h \
    include/WeaponStrategy.h

# Windows 特定设置
win32 {
    CONFIG += windows
    # 解决链接器问题
    QMAKE_LFLAGS += -Wl,--enable-stdcall-fixup
    QMAKE_LFLAGS += -Wl,--enable-auto-import
}

# Debug/Release 配置
CONFIG(debug, debug|release) {
    TARGET = DroneRadarSystemd
    DEFINES += DEBUG
}

CONFIG(release, debug|release) {
    DEFINES += NDEBUG
}

# 输出目录
DESTDIR = build
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc
UI_DIR = $$PWD/build/ui
