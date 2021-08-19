#include <iostream>
#include "net_core.hpp"


int main() {
    asio::io_context io_context;
    Server srv(io_context, 12995);
    srv.async_accept_connection(); // Non-blocking
    io_context.run(); // Blocking until tasks run out (never)

    return 0;
}