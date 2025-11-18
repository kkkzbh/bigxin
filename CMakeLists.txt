cmake_minimum_required(VERSION 4.0.2)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

project(JuXin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "" FORCE)

find_package(Threads REQUIRED)
find_package(Asio CONFIG REQUIRED)
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Network)
find_package(libpqxx CONFIG REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(src)
