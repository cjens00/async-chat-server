#pragma once

/// Client & Server Objects, Asynchronous Free Functions ///

#include <iostream>
#include <optional>
#include <string>
#include <format>
#include <vector>
#include <chrono>
#include <queue>
#include <locale>
#include <unordered_set>

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE

#include <asio.hpp>

using tcp = asio::ip::tcp;
using error_code = asio::error_code;
using message_handler = std::function<void(std::string)>;
using error_handler = std::function<void()>;
using namespace std::placeholders;

class position {
public:
    float x = 0.0;
    float y = 0.0;
    float z = 0.0;

    position() = default;
};

class rotation {
public:
    float x = 0.0;
    float y = 0.0;
    float z = 0.0;

    rotation() = default;
};

class transform {
public:
    transform() : pos(), rot(), transformBuffer{pos.x, pos.y, pos.z, rot.x, rot.y, rot.z} {}

private:
    position pos;
    rotation rot;
    float transformBuffer[6];
};

// Session
class session : public std::enable_shared_from_this<session> {
public:
    // socket is an rvalue reference, we move it
    session(tcp::socket &&socket_) :
            socket(std::move(socket_)) {}

    void start(message_handler &&on_message, error_handler &&on_error) {
        this->on_message = std::move(on_message);
        this->on_error = std::move(on_error);
        async_read();
    }

    void post(const std::string &message) {
        bool idle = outgoing.empty();
        outgoing.push(message);
        if (idle) {
            async_write();
        }
    }

private:

    void async_read() {
        //asio::async_read(socket, readBuffer, std::bind(&session::on_read, shared_from_this(), _1, _2));
        asio::async_read_until(socket, streambuf, "\n", std::bind(&session::on_read, shared_from_this(), _1, _2));
    }

    void on_read(error_code error, std::size_t bytes_transferred) {
        if (!error) {
            std::stringstream message;
            message << socket.remote_endpoint(error) << ": " << std::istream(&streambuf).rdbuf();
            streambuf.consume(bytes_transferred);
            on_message(message.str());
            async_read();
        } else {
            socket.close(error);
            on_error();
        }
    }

    void async_write() {
        asio::async_write(socket, asio::buffer(outgoing.front()),
                          std::bind(&session::on_write, shared_from_this(), _1, _2));
    }

    void on_write(error_code error, std::size_t bytes_transferred) {
        if (!error) {
            outgoing.pop();

            if (!outgoing.empty()) {
                async_write();
            }
        } else {
            socket.close(error);
            on_error();
        }
    }

private:
    enum { buffer_size = 1024 };
    char readArray[buffer_size];
    char writeArray[buffer_size];
    asio::mutable_buffer readBuffer = asio::buffer(readArray, 1024);
    asio::mutable_buffer writeBuffer = asio::buffer(writeArray, 1024);
    tcp::socket socket; // Client's socket
    asio::streambuf streambuf{65535}; // Streambuf for incoming data
    std::queue<std::string> outgoing; // The queue of outgoing messages
    message_handler on_message; // Message handler
    error_handler on_error; // Error handler
};

class server {
public:
    // here we want an asio context reference
    server(asio::io_context &io_context_, std::uint16_t port) : io_context(io_context_), acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {}

    void log(std::string message) {
        std::cout << std::format("[{:%T}] Log: {} [Total Online: {}]\n",
                                 floor<std::chrono::seconds>(std::chrono::system_clock::now()),
                                 message, (int) clients.size());
    }

    void async_accept() {
        socket.emplace(io_context);

        acceptor.async_accept(*socket, [&](error_code error)
        {
            auto client = std::make_shared<session>(std::move(*socket)); // session constructor takes rvalue std::move(*socket)
            client->post("Welcome to chat\n\r");
            post("Client joined.\n\r");
            clients.insert(client);

            // server-side inform of connection
            log("Client Connected.");

            client->start
                    (
                            std::bind(&server::post, this, _1),
                            [&, weak = std::weak_ptr(client)] {
                                if (auto shared = weak.lock(); shared && clients.erase(shared)) {
                                    post("Client disconnected.\n\r");
                                    log("Client Disconnected.");
                                }
                            }
                    );

            async_accept();
        });
    }

    void post(std::string const &message) {
        for (auto &client : clients) {
            client->post(message);
        }
    }

private:
    asio::io_context &io_context; // reference to the io_context object
    tcp::acceptor acceptor;
    std::optional<tcp::socket> socket;
    // A set of connected clients
    std::unordered_set<std::shared_ptr<session>> clients;
};
