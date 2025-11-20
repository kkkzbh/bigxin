

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

# 让 Theme.qml 在 QML 模块中以 singleton 形式注册
set_source_files_properties(client/qml/Theme.qml
        PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

 qt_add_qml_module(client
         URI WeChatClient
         VERSION 1.0
         QML_FILES
         client/qml/Theme.qml
         client/qml/Main/Dialogs/AddFriendDialog.qml
         client/qml/Main/Dialogs/PlusMenu.qml
         client/qml/Main/Dialogs/SettingsDialog.qml
         client/qml/Main/Dialogs/StartGroupDialog.qml
         client/qml/Login/Login.qml
         client/qml/Login/RegisterWindow.qml
         client/qml/Main/Main.qml
         client/qml/Main/Sidebar/Sidebar.qml
         client/qml/Main/ChatList/ChatList.qml
         client/qml/Main/ChatList/ContactListView.qml
         client/qml/Main/ChatArea/ChatArea.qml
         client/qml/Main/ChatArea/ContactDetailView.qml
         RESOURCES
         client/qml/resource/sidebar/chat-3-fill.svg
         client/qml/resource/sidebar/chat-3-line.svg
         client/qml/resource/sidebar/remix-fill.svg
         client/qml/resource/sidebar/settings-line.svg
         client/qml/resource/sidebar/terminal-box-fill.svg
         client/qml/resource/sidebar/terminal-box-line.svg
         client/qml/resource/sidebar/user-fill.svg
         client/qml/resource/sidebar/user-line.svg
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
