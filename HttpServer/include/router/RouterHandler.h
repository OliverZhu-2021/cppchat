#pragma once
#include <string>
#include <memory>
#include <muduo/net/TcpConnection.h>
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
namespace router
{

class RouterHandler
{
public:
    virtual ~RouterHandler() = default;
    virtual void handle(const HttpRequest& req, HttpResponse* resp) = 0;
};

class StreamRouterHandler
{
public:
    virtual ~StreamRouterHandler() = default;
    virtual void handle(const HttpRequest& req, muduo::net::TcpConnectionPtr conn) = 0;
};

} // namespace router
} // namespace http