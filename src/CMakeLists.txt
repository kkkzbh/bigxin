
add_executable(main main.cpp)
target_link_libraries(main PRIVATE asio::asio)

add_library(libchat INTERFACE)

target_include_directories(libchat INTERFACE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(libchat
        INTERFACE
        asio::asio
        libpqxx::pqxx
)

add_executable(server
        server/server.cpp
)

target_link_libraries(server
        PRIVATE
        libchat
)

add_executable(client
        client/client.cpp
)

qt_add_qml_module(client
        URI WeChatClient
        VERSION 1.0
        QML_FILES
        client/qml/Theme.qml
        client/qml/Login/Login.qml
        client/qml/Login/RegisterWindow.qml
        client/qml/Main/Main.qml
        client/qml/Main/Sidebar/Sidebar.qml
        client/qml/Main/ChatList/ChatList.qml
        client/qml/Main/ChatArea/ChatArea.qml
        SOURCES
        ${CMAKE_SOURCE_DIR}/include/login.backend.h
)

target_link_libraries(client
        PRIVATE
        libchat
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
)
