#include "net_core.hpp"

int main() {

    asio::io_context io_context;

    Server srv(io_context, 12995);
    srv.async_accept_connection();
    io_context.run();

    return 0;
}