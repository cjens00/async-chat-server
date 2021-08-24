#pragma once

#include "net_core.hpp"

/// ---------------------------------------
///     Free Function(s)
/// ---------------------------------------

/// Prints message to log
/// TODO: Write to logfile
void Log(std::string s) {
    auto timestamp = floor<std::chrono::seconds>(std::chrono::system_clock::now());
    auto log = fmt::format("{} Log: ", timestamp);
    std::cout << log << s << std::endl;
}

/// Tokenizes a string, used for checking client socket reads for commands, etc.
/// Would be faster to use a vector reference to avoid copying by value afterward,
/// but for now I'll leave it like this.
std::vector<std::string> Tokenize_String(std::string s) {
    std::string token;
    std::vector<std::string> token_list;
    std::for_each(s.begin(), s.end(),
                  [&](auto &c) {
                      if (c != ' ') {
                          token.push_back(c);
                      } else {
                          token_list.push_back(token);
                          token.clear();
                      }
                  });
    return token_list;
}

/// ---------------------------------------
///     Session Implementation
/// ---------------------------------------

/// Session Constructor
Session::Session(tcp::socket &&sock, int id) : socket(std::move(sock)), identifier(id) {
    client_ip_string = initialize_address_as_string();
}

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
        // Check if string is of command format (leading '/')
        // TODO: the problem is not here, but with weak->shared ptr for server obj
        if (std::istream(&stream_buffer).peek() == '/') {
            message << std::istream(&stream_buffer).rdbuf();
            stream_buffer.consume(bytes_transferred);
            command_handler(message.str());
            async_read();
        } else {
            message << socket.remote_endpoint(error) << ": " << std::istream(&stream_buffer).rdbuf();
            stream_buffer.consume(bytes_transferred);
            message_handler(message.str());
            async_read();
        }
    } else {
        socket.close(error);
        error_handler(get_client_id());
    }
}

/// Move error handler from Server
// TODO: typedef these
void Session::set_handlers(std::function<void(int)> &&err_handler,
                           std::function<void(std::string)> &&msg_handler,
                           std::function<void(std::string)> &&cmd_handler) {
    error_handler = std::move(err_handler);
    message_handler = std::move(msg_handler);
    command_handler = std::move(cmd_handler);
}

/// Initializes field client_ip_string after the socket has been opened, private use only.
/// Do not call more than once along with constructor.
std::string Session::initialize_address_as_string() { return socket.remote_endpoint().address().to_string(); }

/// Return string representation of client's IP Address
std::string Session::get_address_as_string() { return client_ip_string; };

/// Sends a string msg to the associated client
void Session::async_send_message(std::string &msg) {
    bool isIdle = message_queue.empty();
    message_queue.push(msg);
    if (isIdle)
        async_write();
}

void Session::async_send_message(const std::string &msg) {
    bool isIdle = message_queue.empty();
    message_queue.push(msg);
    if (isIdle)
        async_write();
}

/// Returns the session's client identifier value
int Session::get_client_id() {
    return identifier;
}

/// Sends a message to this Session's client only
void Session::notify(const std::string &notification) {
    asio::async_write(socket, asio::buffer(notification),
                      [&](asio::error_code error, std::size_t b_tx) {
                          if (error) {
                              socket.close(error);
                              error_handler(get_client_id());
                          }
                      });
}

/// ---------------------------------------
///     Server Implementation
/// ---------------------------------------

/// Constructor
Server::Server(asio::io_context &context, std::uint16_t port)
        : io_context(context), acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
    client_id = 0;
    Log("Server Initialized.");
}

/// Asynchronously accept a new connection
void Server::async_accept_connection() {
    temp_socket.emplace(io_context);
    acceptor.async_accept(*temp_socket, [&](asio::error_code errc) {
        if (!ec) ec = std::make_unique<asio::error_code>(errc);
        on_accept(*temp_socket, errc);
    });
}

/// Notify all clients of new connection, then await another connection
void Server::on_accept(tcp::socket &tmp_socket, asio::error_code error) {
    if (!error) {
        auto new_client = std::make_shared<Session>(std::move(*temp_socket), client_id);
        clients.insert({client_id, new_client});
        new_client->set_handlers([&](int client_id) { on_error(client_id); },
                                 [&](std::string msg) { on_message(msg); },
                                 [&](std::string cmd) { on_command(cmd); });
        new_client->async_session_begin();
        client_id++;
        async_post(fmt::format("User at {} is online\r\n", new_client->initialize_address_as_string()));
        Log(fmt::format("Accepted new connection from {}", new_client->initialize_address_as_string()));
        async_accept_connection();
    } else {
        on_error(-1);
    }
}

bool Server::check_command(std::string &msg) {
    std::string command;
    std::vector<std::string> tokens = Tokenize_String(msg);
    std::cout << "Testing here." << std::endl;
    if (tokens.size() > 1 && tokens[0] == "/server")
        command = tokens[1];
    if (command == "shutdown")
        execute_command(SERVER_COMMAND_SHUTDOWN);
    return true;
}

bool Server::check_command(std::string &&msg_) {
    auto msg = std::move(msg_);
    std::string command;
    std::vector<std::string> tokens = Tokenize_String(msg);
    std::cout << "Testing here." << std::endl;
    if (tokens.size() > 1 && tokens[0] == "server")
        command = tokens[1];
    if (command == "shutdown")
        execute_command(SERVER_COMMAND_SHUTDOWN);
    return true;
}

void Server::execute_command(int command) {
    if (command == SERVER_COMMAND_SHUTDOWN) shutdown_server();
    else if (command == 65533) return; // command 2
    else if (command == 65534) return; // command 3
    else if (command == 65535) return; // command 4 ...etc.
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

/// On Command Handler
void Server::on_command(std::string &command) {
    check_command(command);
}

/// Message handler, writes a received message to all client sockets
void Server::on_message(std::string &msg) {
    async_post(msg);
}

/// Error Handler, also handles disconnects
void Server::on_error(int cid) {
    std::string ec_msg = ec.get()->message(); // TODO: remove get() and check
    int ec_val = ec.get()->value();
    std::string caddr = clients[cid]->get_address_as_string();
    // Attempt to make a new shared ptr for duration of this function
    auto weak = std::weak_ptr(clients[cid]);
    auto shared = weak.lock();
    // If new shared_ptr made, erase old from list of clients
    if (shared)
        clients.erase(cid);
    // If cid == -1, function was called from inside Server object
    if (cid == -1) {
        Log(fmt::format("on_error[server]: {}", ec_msg));
    } else {
        // If there's an error, print it to server log
        if (ec_val) {
            Log(fmt::format("on_error[session]: {}", ec_msg));
            Log(fmt::format("on_error[session]: Client at {} disconnected", caddr));
        } else {
            Log(fmt::format("on_error[session]: No error, Client at {} disconnected", caddr));
        }
    }
    async_post(fmt::format("{} disconnected.\r\n", caddr));
}

// TODO: fix shutdown sequence, after successful d/c of all clients
//  it throws an exception about string iterator
void Server::shutdown_server() {
    {
        Log("Server shutdown sequence initiated. Source:[]");
        async_post(fmt::format("SERVER: The server is shutting down\r\n"));
        std::string notification = "You were disconnected from the server.\r\n";
        for (auto &client: clients) {
            // If statement checks to make sure they're still online
            if (client.second) {
                client.second->notify(notification);
                client.second->socket.shutdown(tcp::socket::shutdown_both);
                client.second->socket.close();
            }
        }
        Log("Server shutdown sequence: client disconnections complete. Stopping service.");
    }
    // acceptor.close();
}

/// Not in use at the moment, not needed for a shutdown sequence anyway
void Server::wipe_client(int cid) {
    if (clients[cid] == nullptr) {
        Log(fmt::format("Client does not exist."));
    } else {
        std::string caddr = clients[cid]->get_address_as_string();
        auto weak = std::weak_ptr(clients[cid]);
        auto shared = weak.lock();
        if (shared)
            clients.erase(cid);
    }
}