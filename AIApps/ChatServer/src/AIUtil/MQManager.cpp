#include"../include/AIUtil/MQManager.h"
#include <cstdlib>

// ------------------- MQManager -------------------
MQManager::MQManager(size_t poolSize)
    : poolSize_(poolSize), counter_(0) {
    const char* host = std::getenv("RABBITMQ_HOST");
    const char* user = std::getenv("RABBITMQ_USER");
    const char* pass = std::getenv("RABBITMQ_PASSWORD");
    const std::string mq_host = host ? host : "localhost";
    const std::string mq_user = user ? user : "guest";
    const std::string mq_pass = pass ? pass : "guest";
    for (size_t i = 0; i < poolSize_; ++i) {
        auto conn = std::make_shared<MQConn>();
        conn->channel = AmqpClient::Channel::Create(mq_host, 5672, mq_user, mq_pass, "/");
        pool_.push_back(conn);
    }
}

void MQManager::publish(const std::string& queue, const std::string& msg) {
    size_t index = counter_.fetch_add(1) % poolSize_;
    auto& conn = pool_[index];

    std::lock_guard<std::mutex> lock(conn->mtx);
    auto message = AmqpClient::BasicMessage::Create(msg);
    conn->channel->BasicPublish("", queue, message);
}

// ------------------- RabbitMQThreadPool -------------------

void RabbitMQThreadPool::start() {
    for (int i = 0; i < thread_num_; ++i) {
        workers_.emplace_back(&RabbitMQThreadPool::worker, this, i);
    }
}

void RabbitMQThreadPool::shutdown() {
    stop_ = true;
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void RabbitMQThreadPool::worker(int id) {
    try {
        const char* user = std::getenv("RABBITMQ_USER");
        const char* pass = std::getenv("RABBITMQ_PASSWORD");
        auto channel = AmqpClient::Channel::Create(
            rabbitmq_host_, 5672,
            user ? user : "guest",
            pass ? pass : "guest",
            "/");
        // set exclusive
        channel->DeclareQueue(queue_name_, false, true, false, false);
        // Prevent channel error: 403: AMQP_BASIC_CONSUME_METHOD caused: ACCESS_REFUSED - queue 
        // 'sql_queue' in vhost '/' in exclusive use
        // std::string consumer_tag = channel->BasicConsume(queue_name_, "");
        std::string consumer_tag = channel->BasicConsume(queue_name_, "", true, false, false);

        channel->BasicQos(consumer_tag, 1); 

        while (!stop_) {
            AmqpClient::Envelope::ptr_t env;
            bool ok = channel->BasicConsumeMessage(consumer_tag, env, 500); // 500ms 
            if (ok && env) {
                std::string msg = env->Message()->Body();
                handler_(msg);          
                channel->BasicAck(env); 
            }
        }

        channel->BasicCancel(consumer_tag);
    }
    catch (const std::exception& e) {
        std::cerr << "Thread " << id << " exception: " << e.what() << std::endl;
    }
}
