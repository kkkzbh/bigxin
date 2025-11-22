

add_library(libchat INTERFACE)

target_include_directories(libchat INTERFACE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(libchat
        INTERFACE
        asio
        libpqxx::pqxx
)

add_executable(server
        server/server.cpp
        server/session/auth.cpp
        server/session/message.cpp
        server/session/friend.cpp
        server/session/conversation.cpp
        server/server/broadcast.cpp
        server/server/push.cpp
        database/connection.cpp
        database/auth.cpp
        database/friend.cpp
        database/conversation.cpp
        database/message.cpp
)

target_link_libraries(server
        PRIVATE
        libchat
        STDEXEC::asioexec_asio
        tbb
)

add_executable(client
        client/client.cpp
        client/backend/network_manager.cpp
        client/backend/message_cache.cpp
        client/backend/protocol_handler.cpp
        client/backend/login_backend.cpp
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
         client/qml/Main/Dialogs/MySpinBox.qml
         client/qml/Main/Dialogs/PlusMenu.qml
         client/qml/Main/Dialogs/SettingsDialog.qml
         client/qml/Main/Dialogs/UserContextMenu.qml
         client/qml/Main/Dialogs/MuteMemberDialog.qml
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
         client/qml/resource/ChatArea/openai.svg
         client/qml/resource/ChatArea/list-unordered.svg
         SOURCES
         ${CMAKE_SOURCE_DIR}/include/login.backend.h
         ${CMAKE_SOURCE_DIR}/include/network_manager.h
         ${CMAKE_SOURCE_DIR}/include/message_cache.h
         ${CMAKE_SOURCE_DIR}/include/protocol_handler.h
)

target_link_libraries(client
        PRIVATE
        libchat
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
)


add_executable(MAN
        main.cpp
)

target_link_libraries(MAN
        PRIVATE
        STDEXEC::asioexec_asio
        tbb
)
