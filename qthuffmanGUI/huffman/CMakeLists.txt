# 指定 C++ 编译器路径
set(CMAKE_CXX_COMPILER "C:/Users/Lenovo/Desktop/MSYS2/ucrt64/bin/g++.exe")
set(CMAKE_C_COMPILER "C:/Users/Lenovo/Desktop/MSYS2/ucrt64/bin/gcc.exe")
cmake_minimum_required(VERSION 3.16)
project(HuffmanGUI VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)      # 自动调用 moc（元对象编译器）
set(CMAKE_AUTOUIC ON)      # 自动调用 uic（界面编译器）
set(CMAKE_AUTORCC ON)      # 自动调用 rcc（资源编译器）

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# 添加可执行文件，包含所有源文件
add_executable(HuffmanGUI
    main.cpp
    mainwindow.cpp
    huffman.cpp
    mainwindow.ui
    mainwindow.h
    huffman.h       # 你的压缩库源文件
)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
# 链接 Qt 库
target_link_libraries(HuffmanGUI Qt6::Core Qt6::Widgets)