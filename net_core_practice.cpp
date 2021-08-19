#pragma once
#include "net_core.hpp"

/// ---------------------------------------
///     Free Function(s)
/// ---------------------------------------

/// Prints message to log
/// TODO: Write to logfile
void Log(std::string s) {
    auto timestamp = floor<std::chrono::seconds>(std::chrono::system_clock::now());
    auto log = std::format("{} Log: ", timestamp);
    std::cout << log << s << std::endl;
}

/// ---------------------------------------
///     Session Implementation
/// ---------------------------------------

/// Session Constructor
Session::Session(tcp::socket &&sock, int id) : socket(std::move(sock)), identifier(id) {}

/// Performs required setup tasks for Session
void Session::async_session_begin() {
    // ok write to client
}

/// Asynchronously write data to the socket
void Session::async_write() {
    asio::async_write(socket, asio::buffer(message_queue.front()),
                      [&](asio::error_code ec, std::size_t b_tx) { handler_write(ec, b_tx); });
}

/// Asynchronously read data from the socket
void Session::async_read() {
    asio::async_read_until(socket, stream_buffer, "\n",
                           [&](asio::error_code ec, std::size_t b_rx){ handler_read(ec, b_rx); });
}

/// Handler called after session begin
void Session::handler_begin() {}

/// Handler called after an async write
void Session::handler_write(asio::error_code error, std::size_t bytes_transferred) {
    if (!error) {
        message_queue.pop();
        if (!message_queue.empty()) { async_write(); }
    } else {
        socket.close(error);
        error_handler();
    }
}

/// Handler called after an async read
void Session::handler_read(asio::error_code error, std::size_t bytes_transferred) {
    if (!error) {
        std::stringstream message;
        message << socket.remote_endpoint(error) << ": " << std::istream(&stream_buffer).rdbuf();
        stream_buffer.consume(bytes_transferred);
        message_handler(message.str());
        async_read();
    } else {
        socket.close(error);
        error_handler();
    }
}

/// Move error handler from Server
void Session::set_handlers(std::function<void()> &&err_handler, std::function<void(std::string)> &&msg_handler) {
    error_handler = std::move(err_handler);
    message_handler = std::move(msg_handler);
}

/// Return String representation of client's IP Address
std::string Session::get_address_as_string()
{ return socket.remote_endpoint().address().to_string(); }

/// Sends a string msg to the associated client
void Session::async_send_message(const std::string &msg) {
    message_queue.push(msg);
    if(message_queue.empty())
        async_write();
}

/// Returns the session's client identifier value
int Session::get_client_id() {
    return identifier;
}

/// ---------------------------------------
///     Server Implementation
/// ---------------------------------------

/// Constructor
Server::Server(asio::io_context &context, std::uint16_t port)
: io_context(context), acceptor(io_context, tcp::endpoint(tcp::v4(), port)), client_id(0) {
    Log("Server Initialized.");
}

/// Asynchronously accept a new connection
void Server::async_accept_connection() {
    temp_socket.emplace(io_context);
    acceptor.async_accept(*temp_socket, [&](asio::error_code ec) {
        auto new_client = std::make_shared<Session>(std::move(*temp_socket), client_id);
        clients[client_id] = new_client;
        // TODO: Fix problem with handlers, w/o err handler there can be no debugging, just hangs on connect
        new_client->set_handlers(
                [&]() { on_error(client_id); },
                [&](std::string msg) { on_message(msg); });

        client_id++;

        // TODO: async_session_begin() establishes relevant (to be impl.) client data
        // new_client->async_session_begin(); not in use atm

        Log(std::format("Accepted new connection from {}", new_client->get_address_as_string()));
        on_accept(std::format("User at {} is online", new_client->get_address_as_string()));
    });
}

/// Notify all clients of new connection, then await another connection
void Server::on_accept(const std::string &announce_msg) {
    async_post(announce_msg);
    async_accept_connection();
}

/// Make an announcement 'msg' to all connected clients
void Server::async_post(const std::string &msg) {
    for(auto &client: clients) {
        client.second->async_send_message(msg);
    }
}

/// Message handler, writes a received message to all client sockets
void Server::on_message(const std::string &msg) {
    async_post(msg);
}

/// Error Handler, also handles disconnects
void Server::on_error(int cid) {
    auto weak = std::weak_ptr(clients[cid]);
    auto shared = weak.lock();
    std::string caddr = shared->get_address_as_string();
    if(shared) clients.erase(cid);
    Log(std::format("Connection from {} lost", caddr));
}
