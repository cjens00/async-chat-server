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
    async_read();
}

/// Asynchronously write data to the socket
void Session::async_write() {
    asio::async_write(socket, asio::buffer(message_queue.front()),
                      [&](asio::error_code ec, std::size_t b_tx) { handler_write(ec, b_tx); });
}

/// Asynchronously read data from the socket
void Session::async_read() {
    asio::async_read_until(socket, stream_buffer, "\n",
                           [&](asio::error_code ec, std::size_t b_rx) { handler_read(ec, b_rx); });
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
        error_handler(get_client_id());
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
        error_handler(get_client_id());
    }
}

/// Move error handler from Server
void Session::set_handlers(std::function<void(int)> &&err_handler, std::function<void(std::string)> &&msg_handler) {
    error_handler = std::move(err_handler);
    message_handler = std::move(msg_handler);
}

/// Return String representation of client's IP Address
std::string Session::get_address_as_string() { return socket.remote_endpoint().address().to_string(); }

/// Sends a string msg to the associated client
void Session::async_send_message(std::string &msg) {
    bool isIdle = message_queue.empty();
    message_queue.push(msg);
    if (isIdle)
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
        : io_context(context), acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
            client_id(0) {
    Log("Server Initialized.");
}

/// Asynchronously accept a new connection
void Server::async_accept_connection() {
    temp_socket.emplace(io_context);
    acceptor.async_accept(*temp_socket, [&](asio::error_code errc) {
        if (!ec) ec = std::make_unique<asio::error_code>(errc);
        on_accept(*temp_socket, errc); });
}

/// Notify all clients of new connection, then await another connection
void Server::on_accept(tcp::socket &tmp_socket, asio::error_code error) {
    if (!error) {
        auto new_client = std::make_shared<Session>(std::move(*temp_socket), client_id);

        clients.insert({client_id, new_client});
        // TODO: Fix problem with handlers, w/o err handler there can be no debugging, just hangs on connect
        new_client->set_handlers([&](int client_id) { on_error(client_id); },
                                 [&](std::string msg) { on_message(msg); });
        new_client->async_session_begin();
        client_id++;
        async_post(std::format("User at {} is online", new_client->get_address_as_string()));
        Log(std::format("Accepted new connection from {}", new_client->get_address_as_string()));
        async_accept_connection();
    } else {
        on_error(-1);
    }
}

/// Make an announcement 'msg' to all connected clients
void Server::async_post(std::string &msg) {
    for (auto &client: clients) {
        client.second->async_send_message(msg);
    }
}

/// rvalue overload
void Server::async_post(std::string &&msg) {
    std::string msg_ = std::move(msg);
    for (auto &client: clients) {
        client.second->async_send_message(msg_);
    }
}

/// Message handler, writes a received message to all client sockets
void Server::on_message(std::string &msg) {
    async_post(msg);
}

/// Error Handler, also handles disconnects
void Server::on_error(int cid) {
    if (cid == -1) {
        std::cout << "on_error[server]  : " << ec->message() << std::endl;
    } else {
        std::cout << "on_error[session] : " << ec->message() << std::endl;
        auto weak = std::weak_ptr(clients[cid]);
        auto shared = weak.lock();
        std::string caddr = shared->get_address_as_string();
        if (shared) { clients.erase(cid); }
        Log(std::format("Connection from {} lost", caddr));
    }
}
