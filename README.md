# async-chat-server
Example asynchronous chat server in C++ using standalone Asio and TCP sockets.
- Uses C++20 and compiled with MSVC.
- Dependencies: 
    - Asio, http://think-async.com/Asio

Highlights:
- Attempts to keep lambda usage to a minimum (I found that many tutorials were using large, 20-line+ lambdas which were difficult or impossible to debug and learn from).
