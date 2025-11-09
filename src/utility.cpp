module;

#include <boost/asio.hpp>

export module utility;

// 通用工具与常量（全项目可用）
namespace util
{
    namespace asio = boost::asio;
    export auto inline constexpr use_awaitable_noexcept = asio::as_tuple(asio::use_awaitable);
}
