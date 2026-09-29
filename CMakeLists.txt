cmake_minimum_required(VERSION 3.16)
project(CameraTest_Qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH "D:/Qt/6.11.1/msvc2022_64")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Gui PrintSupport)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
qt_standard_project_setup()

add_executable(CameraTest_Qt
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
    emva_common.h
    emva_gain.h
    emva_noise.h
    emva_dark.h
    emva_snr_dr.h
    emva1288_calculator.h
    qcustomplot.cpp   
    qcustomplot.h 
)

target_link_libraries(CameraTest_Qt PRIVATE 
    Qt6::Widgets 
    Qt6::Charts 
    Qt6::Gui
    Qt6::PrintSupport
)

add_custom_command(TARGET CameraTest_Qt POST_BUILD
    COMMAND "D:/Qt/6.11.1/msvc2022_64/bin/windeployqt.exe" 
            --debug 
            --no-translations 
            --no-system-d3d-compiler 
            --no-opengl-sw 
            "$<TARGET_FILE:CameraTest_Qt>"
    COMMENT "Automatically deploying Qt DLLs and plugins..."
)