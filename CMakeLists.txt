cmake_minimum_required(VERSION 4.0.1)

set(vcpkg_root "$ENV{HOME}/vcpkg")

# Auto-hook vcpkg-configuration.json via VCPKG_ROOT for manifest mode
set(CMAKE_TOOLCHAIN_FILE "${vcpkg_root}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)

#set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "" FORCE)
set(VCPKG_TARGET_TRIPLET "x64-linux-ftxui-modules" CACHE STRING "" FORCE)
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/vcpkg_triplets" CACHE STRING "" FORCE)

set(ENV{CC}  /usr/lib/llvm-21/bin/clang)
set(ENV{CXX} /usr/lib/llvm-21/bin/clang++)
set(ENV{CXXFLAGS} "-stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind")
set(ENV{LDFLAGS}  "-stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld")
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED "CC;CXX;CXXFLAGS;LDFLAGS" CACHE STRING "" FORCE)
set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "" FORCE)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")

set(CMAKE_CXX_FLAGS_INIT           "-stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld")

project(JuXin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)

set(CMAKE_CXX_MODULE_STD ON)

find_package(Boost CONFIG REQUIRED COMPONENTS asio)
find_package(Threads REQUIRED)
find_package(ftxui CONFIG REQUIRED)


# Qt (for GUI/QML)
# 使用者通过 -DCMAKE_PREFIX_PATH=... 指向本机 Qt 安装
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "/opt/Qt/6.9.0/gcc_64")
find_package(Qt6 COMPONENTS Quick Qml QuickControls2 REQUIRED)
qt_policy(SET QTP0001 NEW)
qt_standard_project_setup()
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/opt/Qt/6.9.0/gcc_64/plugins")

find_package(Protobuf CONFIG REQUIRED)

add_library(libchat)

target_sources(libchat
        PUBLIC
        FILE_SET CXX_MODULES
        FILES
        src/utility.cpp
        src/core.cpp
        src/protocol/protocol.cpp
        src/server/core.cpp
        src/client/core.cpp
        src/client/cli/core.cpp
        src/client/cli/command.cpp
        src/client/cli/ui.cpp
)

target_compile_definitions(libchat PRIVATE
        BOOST_ASIO_NO_EXCEPTIONS
        BOOST_ASIO_NO_DEPRECATED
        BOOST_ASIO_NO_DYNAMIC_BUFFER_V1
        BOOST_ASIO_NO_TS_EXECUTORS
)

target_link_libraries(libchat PRIVATE
        Boost::asio
        Threads::Threads
)

add_executable(main
        main.cpp
)
target_link_libraries(main PRIVATE
        ftxui::component
)


# Tests
enable_testing()

add_executable(server
        src/server/server.cpp
)
target_link_libraries(server PRIVATE libchat)

add_executable(client
        src/client/cli/client.cpp
)
target_link_libraries(client PRIVATE
        libchat
        ftxui::component
        ftxui::dom
        ftxui::screen
)

# GUI (Qt/QML) — entry: src/client/gui/client.cpp
add_executable(client_gui
        src/client/gui/client.cpp
        src/client/gui/chat_client_proxy.h
        src/client/gui/chat_client_proxy.cpp
)

qt_add_qml_module(client_gui
        URI chat.gui
        VERSION 1.0
        QML_FILES
            src/client/gui/qml/App.qml
            src/client/gui/qml/Theme.qml
            src/client/gui/qml/NavRail.qml
            src/client/gui/qml/ConversationList.qml
            src/client/gui/qml/ConversationItem.qml
            src/client/gui/qml/ChatPane.qml
            src/client/gui/qml/ChatHeader.qml
            src/client/gui/qml/MessageList.qml
            src/client/gui/qml/MessageBubble.qml
            src/client/gui/qml/ComposerBar.qml
        RESOURCES
            JX_CUT.png
            JX_CUT.svg
)

target_link_libraries(client_gui PRIVATE
        libchat
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
)
