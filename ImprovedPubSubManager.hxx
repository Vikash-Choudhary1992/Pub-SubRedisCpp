
#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <sw/redis++/redis++.h>

namespace rcs {

class ImprovedPubSubManager {
public:
    static ImprovedPubSubManager& getInstance();

    void publishOverChannel(const std::string& channel, const std::string& message);

    void subscribe(const std::string& channel);
    void unsubscribe(const std::string& channel);
    void onMessage(std::function<void(const std::string&, const std::string&)> handler);
    void startConsuming();
    void stopConsuming();

private:
    ImprovedPubSubManager();
    ~ImprovedPubSubManager();

    ImprovedPubSubManager(const ImprovedPubSubManager&) = delete;
    ImprovedPubSubManager& operator=(const ImprovedPubSubManager&) = delete;

    void initRedis();
    void consumeLoop();

    std::shared_ptr<sw::redis::RedisCluster> mRedisCluster;
    std::unique_ptr<sw::redis::Subscriber> mSubscriber;
    std::function<void(const std::string&, const std::string&)> mMessageHandler;

    std::mutex mRedisMutex;
    std::thread mConsumeThread;
    std::atomic<bool> mRunning{false};
};

} // namespace rcs
