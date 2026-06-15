#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include <muduo/net/TcpConnection.h>

class ChatServer;

class ChatCreateAndStreamHandler : public http::router::StreamRouterHandler {
public:
    explicit ChatCreateAndStreamHandler(ChatServer* server) : server_(server) {}
    void handle(const http::HttpRequest& req, muduo::net::TcpConnectionPtr conn) override;
private:
    ChatServer* server_;
};
