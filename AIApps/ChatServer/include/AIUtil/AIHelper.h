#pragma once
#include <string>
#include <vector>
#include <utility>
#include <functional>
#include <curl/curl.h>
#include <iostream>
#include <sstream>

#include "../../../../HttpServer/include/utils/JsonUtil.h"
#include"../../../../HttpServer/include/utils/MysqlUtil.h"

#include"AIFactory.h"
#include"AIConfig.h"
#include"AIToolRegistry.h"


//这边封装curl去访问对阿里的模型
class AIHelper {
public:
    // 构造函数，初始化API Key
    AIHelper();

    void setStrategy(std::shared_ptr<AIStrategy> strat);

    // 添加一条消息
    void addMessage(int userId, const std::string& userName, bool is_user, const std::string& userInput, std::string sessionId);
    // 恢复一条消息
    void restoreMessage(const std::string& userInput, long long ms);

    // 发送聊天消息，返回AI的响应内容（阻塞，完整响应）
    std::string chat(int userId, std::string userName, std::string sessionId, std::string userQuestion, std::string modelType);

    // 流式发送聊天消息，token 逐步通过 onChunk 回调推送
    void chatStream(int userId, std::string userName, std::string sessionId,
                    std::string userQuestion, std::string modelType,
                    std::function<void(const std::string& chunk)> onChunk,
                    std::function<void()> onDone,
                    std::function<void(const std::string& err)> onError);

    // 可选：发送自定义请求体
    json request(const json& payload);

    std::vector<std::pair<std::string, long long>> GetMessages();

private:
    std::string escapeString(const std::string& input);
    void pushMessageToMysql(int userId, const std::string& userName, bool is_user, const std::string& userInput, long long ms,std::string sessionId);

    // 阻塞式 curl，返回完整 JSON
    json executeCurl(const json& payload);
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    // 流式 curl，每收到一个 delta 就调用 ctx->onChunk
    struct StreamContext {
        std::string lineBuffer;
        std::function<void(const std::string&)> onChunk;
        std::function<void()> onDone;
        bool done = false;
        std::shared_ptr<AIStrategy> strategy;
    };
    void executeCurlStream(const json& payload, StreamContext* ctx);
    static size_t StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

private:
    std::shared_ptr<AIStrategy> strategy;

    //一个用户针对一个AIHelper，messages存放用户的历史对话
    //偶数下标代表用户的信息，奇数下标是ai返回的内容
    //后者代表时间戳
    std::vector<std::pair<std::string, long long>> messages;
};
