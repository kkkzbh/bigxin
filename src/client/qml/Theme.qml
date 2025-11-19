pragma Singleton
import QtQml
import QtQuick

// 集中管理应用的常用主题颜色与尺寸（全局单例）
QtObject {
    // 窗口与顶栏
    property color windowBackground: "#111111"
    property color topBarBackground: "#111111"

    // 侧边栏
    property color sidebarBackground: "#151515"
    property color sidebarButtonHover: "#292929"

    // 会话列表
    property color chatListBackground: "#18191c"
    property color chatListItemNormal: "#18191c"
    property color chatListItemHover: "#222326"
    property color chatListItemSelected: "#202124"
    property color chatListItemSelectedHover: "#2a2c30"

    // 对话区域
    property color chatAreaBackground: "#121314"

    // 分隔线
    property color separatorVertical: "#2A2A2A"
    property color separatorHorizontal: "#303030"

    // 搜索框
    property color searchBoxBackground: "#26272b"
    property color searchIcon: "#7b7d82"
    property color searchText: "#f5f5f5"
    property color searchPlaceholder: "#666666"

    // 文本常用颜色
    property color textPrimary: "#f5f5f5"
    property color textSecondary: "#9a9b9f"
    property color textMuted: "#76777b"

    // 气泡与按钮
    property color bubbleOther: "#26272b"
    property color bubbleMine: "#1aad19"
    property color bubbleOtherText: "#f5f5f5"
    property color bubbleMineText: "#ffffff"
    property color sendButtonEnabled: "#1aad19"
    property color sendButtonDisabled: "#252525"

    // 卡片与面板
    property color cardBackground: "#18191c"
    property color cardBorder: "#2a2b2f"
    property color panelBackground: "#202124"

    // 主按钮
    property color primaryButton: sendButtonEnabled
}
