cmake_minimum_required(VERSION 3.16)

project(nb VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort AxContainer)

qt_add_executable(nb
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

target_link_libraries(nb PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::AxContainer
)
