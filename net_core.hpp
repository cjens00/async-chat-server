/// Client & Server Objects, Asynchronous Free Functions                ///
/// Practicing with move semantics and strange, ASIO-style programming   ///
#pragma once

#include <iostream>
#include <functional>
#include <optional>
#include <string>
#include <format>
#include <vector>
#include <queue>
#include <map>

#ifdef _WIN32
    #define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
#include <asio.hpp>

using tcp = asio::ip::tcp;
using namespace std::placeholders;

class Server;
class Session;

class Session : public std::enable_shared_from_this<Session> {
    friend Server;
public:
    explicit Session(tcp::socket &&sock, int id);
    void async_session_begin();

    void async_write();
    void async_read();

    void handler_begin();
    void handler_write(asio::error_code error, std::size_t bytes_transferred);
    void handler_read(asio::error_code error, std::size_t bytes_transferred);
    void set_handlers(std::function<void(int)> &&err_handler, std::function<void(std::string)> &&msg_handler);

    void async_send_message(std::string &msg);
    std::string initialize_address_as_string();
    std::string get_address_as_string();
    int get_client_id();
private:
    int identifier;
    tcp::socket socket;
    asio::streambuf stream_buffer;
    std::queue<std::string> message_queue;
    std::function<void(int)> error_handler;
    std::function<void(std::string)> message_handler;
    std::string client_ip_string;
};

class Server {
    friend Session;
public:
    Server(asio::io_context &context, std::uint16_t port);
    void async_accept_connection();
    void async_post(std::string &msg);
    void async_post(std::string &&msg);
private:
    void on_accept(tcp::socket &tmp_socket, asio::error_code error);
    void on_error(int cid);
    void on_message(std::string &msg);
private:
    asio::io_context &io_context;
    std::unique_ptr<asio::error_code> ec;
    tcp::acceptor acceptor;
    std::optional<tcp::socket> temp_socket;
    int client_id;
    std::map<int, std::shared_ptr<Session>> clients;
};