#pragma once

#include "benchmark_config.h"
#include "benchmark_client.h"

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <vector>

namespace benchmark
{
    namespace asio = boost::asio;

    /// \brief 账号信息
    struct AccountInfo
    {
        std::string account;      // 账号名
        std::string user_id;      // 登录后获取的 userId
        std::size_t group_index;  // 所属群聊索引
    };

    /// \brief 群聊信息
    struct GroupInfo
    {
        std::string name;             // 群聊名称
        std::string conversation_id;  // 群聊 ID
        std::vector<std::string> member_ids;  // 成员 userId 列表
    };

    /// \brief 账号管理器，负责创建账号和群聊
    class AccountManager
    {
    public:
        /// \brief 构造函数
        /// \param io io_context 引用
        /// \param config 配置对象
        AccountManager(asio::io_context& io, Config const& config);

        /// \brief 执行完整的初始化设置（注册+登录+建群），并保存结果到文件
        auto setup() -> asio::awaitable<bool>;

        /// \brief 从文件加载已保存的数据（用于 connect/message 模式）
        auto load_from_file() -> bool;

        /// \brief 保存数据到文件
        auto save_to_file() -> bool;

        /// \brief 获取账号列表（可修改，用于加载数据）
        [[nodiscard]] auto accounts() -> std::vector<AccountInfo>& { return accounts_; }
        [[nodiscard]] auto accounts() const -> std::vector<AccountInfo> const& { return accounts_; }

        /// \brief 获取群聊列表（可修改，用于加载数据）
        [[nodiscard]] auto groups() -> std::vector<GroupInfo>& { return groups_; }
        [[nodiscard]] auto groups() const -> std::vector<GroupInfo> const& { return groups_; }

        /// \brief 获取观察者账号信息（可修改，用于加载数据）
        [[nodiscard]] auto observer() -> AccountInfo& { return observer_; }
        [[nodiscard]] auto observer() const -> AccountInfo const& { return observer_; }

        /// \brief 根据账号索引获取所属的群聊 conversation_id
        [[nodiscard]] auto get_conversation_id(std::size_t account_index) const -> std::string;

    private:
        /// \brief 注册所有账号
        auto register_accounts() -> asio::awaitable<bool>;

        /// \brief 登录所有账号获取 userId
        auto login_accounts() -> asio::awaitable<bool>;

        /// \brief 创建群聊并添加成员
        auto create_groups() -> asio::awaitable<bool>;

        asio::io_context& io_;
        Config const& config_;

        std::vector<AccountInfo> accounts_;
        std::vector<GroupInfo> groups_;
        AccountInfo observer_;
    };

} // namespace benchmark
