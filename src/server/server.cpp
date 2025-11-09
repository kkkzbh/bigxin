 
 
// Server executable entry (non-module TU)
import std;
import server.core;

auto main() -> int
{
    auto cfg = read_server_config_from_env();
    auto srv = Server{ cfg };
    srv.run();
    return 0;
}
