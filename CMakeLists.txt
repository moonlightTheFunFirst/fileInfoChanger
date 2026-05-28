cmake_minimum_required(VERSION 3.21)

project(fileInfoChanger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(fileInfoChanger
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/FileInfo.h
    src/FileScanner.cpp
    src/FileScanner.h
    src/TextInspector.cpp
    src/TextInspector.h
)

target_link_libraries(fileInfoChanger PRIVATE Qt6::Widgets)
