import QtQuick

QtObject {
    // 色板（暗色主题）
    readonly property color bgNav: "#24262B"        // UI-KNOB: 左侧导航背景色
    readonly property color bgList: "#1F2023"       // UI-KNOB: 会话列表背景色
    readonly property color bgChat: "#121314"       // UI-KNOB: 聊天区背景色
    readonly property color bgSearch: "#2A2B2F"     // UI-KNOB: 搜索/输入等容器底色
    readonly property color textPrimary: "#E6E6E6"  // UI-KNOB: 主文本颜色
    readonly property color textSecondary: "#8B8B8B"// UI-KNOB: 次文本颜色
    readonly property color bubbleIn: "#2E2E2E"      // UI-KNOB: 入站气泡颜色
    readonly property color bubbleOut: "#07C160"     // UI-KNOB: 出站气泡颜色
    readonly property color bubbleOutText: "#0B1F14" // UI-KNOB: 出站气泡内文字颜色

    // 尺寸（以 dp 表示的像素）
    readonly property int railW: 62          // UI-KNOB,HOT-PARAM(require-approval): 导航栏基准宽度（与 sideScale 相乘）
    readonly property int listW: 210         // UI-KNOB,HOT-PARAM(require-approval): 会话列表基准宽度（与 sideScale 相乘）
    readonly property int headerH: 84        // UI-KNOB,HOT-PARAM(require-approval): 聊天标题栏高度
    readonly property int composerMinH: 52   // UI-KNOB,HOT-PARAM(require-approval): 输入区最小高度
    readonly property int corner: 8          // UI-KNOB,HOT-PARAM(require-approval): 统一圆角半径
    readonly property int gap: 8             // UI-KNOB: 统一间距基准（非热参数）
}
