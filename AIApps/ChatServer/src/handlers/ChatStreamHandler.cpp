#include "../include/handlers/ChatStreamHandler.h"
#include "../include/ChatServer.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"
#include "../include/AIUtil/AIHelper.h"

// Build the SSE HTTP header block (sent once at stream start)
static std::string makeSseHeaders() {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/event-stream\r\n"
           "Cache-Control: no-cache\r\n"
           "Connection: keep-alive\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Credentials: true\r\n"
           "\r\n";
}

// Serialize one SSE data line:  data: {"content":"..."}\n\n
static std::string makeSseChunk(const std::string& content) {
    json data;
    data["content"] = content;
    return "data: " + data.dump() + "\n\n";
}

void ChatStreamHandler::handle(const http::HttpRequest& req, muduo::net::TcpConnectionPtr conn)
{
    // Use a temporary HttpResponse only to satisfy getSession's signature;
    // it may refresh the session cookie, but we never send those headers here.
    http::HttpResponse tempResp;
    auto session = server_->getSessionManager()->getSession(req, &tempResp);

    if (session->getValue("isLoggedIn") != "true") {
        conn->send("HTTP/1.1 401 Unauthorized\r\n"
                   "Content-Type: application/json\r\nContent-Length: 27\r\nConnection: close\r\n\r\n"
                   "{\"error\":\"Unauthorized\"}");
        conn->shutdown();
        return;
    }

    int userId = std::stoi(session->getValue("userId"));
    std::string username = session->getValue("username");

    std::string userQuestion, modelType, sessionId;
    auto body = req.getBody();
    if (!body.empty()) {
        try {
            auto j = json::parse(body);
            if (j.contains("question"))  userQuestion = j["question"];
            if (j.contains("sessionId")) sessionId    = j["sessionId"];
            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        } catch (...) {}
    }

    std::shared_ptr<AIHelper> aiHelper;
    {
        std::lock_guard<std::mutex> lock(server_->mutexForChatInformation);
        auto& userSessions = server_->chatInformation[userId];
        if (userSessions.find(sessionId) == userSessions.end()) {
            userSessions.emplace(sessionId, std::make_shared<AIHelper>());
        }
        aiHelper = userSessions[sessionId];
    }

    conn->send(makeSseHeaders());

    aiHelper->chatStream(
        userId, username, sessionId, userQuestion, modelType,
        [conn](const std::string& chunk) {
            conn->send(makeSseChunk(chunk));
        },
        [conn]() {
            conn->send("data: [DONE]\n\n");
            conn->shutdown();
        },
        [conn](const std::string& err) {
            json errData;
            errData["error"] = err;
            conn->send("data: " + errData.dump() + "\n\n");
            conn->send("data: [DONE]\n\n");
            conn->shutdown();
        }
    );
}
