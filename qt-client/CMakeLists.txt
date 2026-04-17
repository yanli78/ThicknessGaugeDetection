cmake_minimum_required(VERSION 3.16)

project(ThicknessGauge VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort)

# ===================== 自动下载、编译 QXlsx（推荐！无需手动下载） =====================
include(FetchContent)
FetchContent_Declare(
    QXlsx
    GIT_REPOSITORY https://github.com/QtExcel/QXlsx.git
    GIT_TAG master
    SOURCE_SUBDIR QXlsx
)
# 自动下载 + 编译 + 链接
FetchContent_MakeAvailable(QXlsx)
set(QXLSX_LIB QXlsx::QXlsx)

# ====================================================================================

qt_add_executable(ThicknessGauge WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    setting.cpp
    setting.h
    setting.ui
    about.cpp
    about.h
    about.ui
)

target_link_libraries(ThicknessGauge PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    ${QXLSX_LIB}
)

# 保留宏定义，你的代码无需修改
if(QXLSX_LIB)
    target_compile_definitions(ThicknessGauge PRIVATE HAS_QXLSX)
endif()
