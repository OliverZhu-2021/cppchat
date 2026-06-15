#include "../include/handlers/ChatCreateAndStreamHandler.h"
#include "../include/ChatServer.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"
#include "../include/AIUtil/AIHelper.h"
#include "../include/AIUtil/AISessionIdGenerator.h"

static std::string makeSseHeaders() {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/event-stream\r\n"
           "Cache-Control: no-cache\r\n"
           "Connection: keep-alive\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Credentials: true\r\n"
           "\r\n";
}

static std::string makeSseChunk(const std::string& content) {
    json data;
    data["content"] = content;
    return "data: " + data.dump() + "\n\n";
}

void ChatCreateAndStreamHandler::handle(const http::HttpRequest& req, muduo::net::TcpConnectionPtr conn)
{
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

    std::string userQuestion, modelType;
    auto body = req.getBody();
    if (!body.empty()) {
        try {
            auto j = json::parse(body);
            if (j.contains("question")) userQuestion = j["question"];
            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        } catch (...) {}
    }

    AISessionIdGenerator generator;
    std::string sessionId = generator.generate();

    std::shared_ptr<AIHelper> aiHelper;
    {
        std::lock_guard<std::mutex> lock(server_->mutexForChatInformation);
        auto& userSessions = server_->chatInformation[userId];
        userSessions.emplace(sessionId, std::make_shared<AIHelper>());
        server_->sessionsIdsMap[userId].push_back(sessionId);
        aiHelper = userSessions[sessionId];
    }

    conn->send(makeSseHeaders());

    // Send the new sessionId as the very first SSE event so the frontend can
    // register it before any content arrives.
    json sessionEvent;
    sessionEvent["sessionId"] = sessionId;
    conn->send("data: " + sessionEvent.dump() + "\n\n");

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
