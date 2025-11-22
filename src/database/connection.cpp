#include <database/connection.h>
#include <chrono>
#include <mutex>
#include <random>

namespace database
{
    auto make_connection() -> pqxx::connection
    {
        return pqxx::connection{ "service=chatdb" };
    }

    auto generate_random_display_name() -> std::string
    {
        static std::mt19937_64 engine(
            static_cast<std::mt19937_64::result_type>(
                std::chrono::steady_clock::now().time_since_epoch().count()
            )
        );
        static std::mutex mutex;
        std::lock_guard lock{ mutex };

        auto number = std::uniform_int_distribution<int>{ 100000, 999999 }(engine);
        return "微信用户" + std::to_string(number);
    }
} // namespace database
