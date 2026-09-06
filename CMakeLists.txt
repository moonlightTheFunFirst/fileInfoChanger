cmake_minimum_required(VERSION 3.21)

project(fileInfoChanger LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Concurrent Widgets)

add_executable(fileInfoChanger WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/FileInfo.h
    src/FileScanner.cpp
    src/FileScanner.h
    src/TextInspector.cpp
    src/TextInspector.h
    src/TextConverter.cpp
    src/TextConverter.h
    resources.qrc
    app.rc
)

target_link_libraries(fileInfoChanger PRIVATE Qt6::Concurrent Qt6::Widgets)

option(FIC_BUILD_TESTS "Build regression tests" OFF)
if(FIC_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(review_tests
        tests/ReviewRegressionTests.cpp
        src/MainWindow.cpp src/MainWindow.h
        src/FileScanner.cpp src/TextInspector.cpp src/TextConverter.cpp
        resources.qrc
    )
    target_include_directories(review_tests PRIVATE src)
    target_link_libraries(review_tests PRIVATE Qt6::Test Qt6::Widgets Qt6::Concurrent)
    set_target_properties(review_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME review_regressions COMMAND review_tests -platform offscreen)
endif()
