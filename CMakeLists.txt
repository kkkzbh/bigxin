cmake_minimum_required(VERSION 4.0.2)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

project(JuXin LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 抑制 stdexec 外部依赖库的 C++26 逗号省略警告
add_compile_options(-Wno-deprecated-variadic-comma-omission)

set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "" FORCE)

include(cmake/CPM.cmake)

find_package(Threads REQUIRED)
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Network)
find_package(boost_asio CONFIG REQUIRED)
find_package(boost_mysql CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

CPMAddPackage(
        NAME stdexec
        GITHUB_REPOSITORY NVIDIA/stdexec
        GIT_TAG 57a03175f229944d64f1c2fb536d3194304dd354
        OPTIONS
            "STDEXEC_ENABLE_ASIO ON"
            "STDEXEC_ASIO_IMPLEMENTATION boost"
)

qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(src)
