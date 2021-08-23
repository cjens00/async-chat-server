#include "net_core.hpp"

int main() {
    asio::io_context io_context;

    auto shared_srv = std::make_shared<Server>(io_context, 12995);
    shared_srv->init();
    shared_srv->async_accept_connection();
    io_context.run();

    return 0;
}