
// ImprovedPubSubManager.cxx
#include "ImprovedPubSubManager.hxx"
#include <chrono>
#include <thread>
#include <iostream>

#define MAX_RETRIES 3

namespace rcs {

ImprovedPubSubManager::ImprovedPubSubManager() {
    initRedis();
}

ImprovedPubSubManager::~ImprovedPubSubManager() {
    stopConsuming();
    // Smart pointers will clean up
}

ImprovedPubSubManager& ImprovedPubSubManager::getInstance() {
    static ImprovedPubSubManager instance;
    return instance;
}

void ImprovedPubSubManager::initRedis() {
    try {
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.keep_alive = true;
        opts.connect_timeout = std::chrono::milliseconds(200);

        sw::redis::ConnectionPoolOptions pool_opts;
        pool_opts.size = 2;
        pool_opts.wait_timeout = std::chrono::milliseconds(200);

        mRedisCluster = std::make_shared<sw::redis::RedisCluster>(opts, pool_opts);
        mSubscriber = std::make_unique<sw::redis::Subscriber>(mRedisCluster->subscriber());
    } catch (const sw::redis::Error& e) {
        std::cerr << "Redis connection failed: " << e.what() << std::endl;
    }
}

void ImprovedPubSubManager::publishOverChannel(const std::string& channel, const std::string& message) {
    std::lock_guard<std::mutex> lock(mRedisMutex);
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        try {
            if (mRedisCluster) {
                mRedisCluster->publish(channel, message);
                return;
            }
        } catch (const sw::redis::Error& e) {
            std::cerr << "Attempt " << attempt + 1 << " failed to publish: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (attempt + 1)));
        }
    }
    std::cerr << "Failed to publish after " << MAX_RETRIES << " attempts" << std::endl;
}

void ImprovedPubSubManager::subscribe(const std::string& channel) {
    std::lock_guard<std::mutex> lock(mRedisMutex);
    if (mSubscriber) {
        mSubscriber->subscribe(channel);
    }
}

void ImprovedPubSubManager::unsubscribe(const std::string& channel) {
    std::lock_guard<std::mutex> lock(mRedisMutex);
    if (mSubscriber) {
        mSubscriber->unsubscribe(channel);
    }
}

void ImprovedPubSubManager::onMessage(std::function<void(const std::string&, const std::string&)> handler) {
    std::lock_guard<std::mutex> lock(mRedisMutex);
    mMessageHandler = std::move(handler);
    if (mSubscriber) {
        mSubscriber->on_message([this](std::string channel, std::string msg) {
            if (mMessageHandler) {
                mMessageHandler(channel, msg);
            }
        });
    }
}

void ImprovedPubSubManager::startConsuming() {
    if (mRunning.load()) return;
    mRunning = true;
    mConsumeThread = std::thread(&ImprovedPubSubManager::consumeLoop, this);
}

void ImprovedPubSubManager::stopConsuming() {
    mRunning = false;
    if (mConsumeThread.joinable()) {
        mConsumeThread.join();
    }
}

void ImprovedPubSubManager::consumeLoop() {
    while (mRunning.load()) {
        try {
            if (mSubscriber) {
                mSubscriber->consume();
            }
        } catch (const sw::redis::TimeoutError&) {
            // Ignore and continue
        } catch (const sw::redis::Error& e) {
            std::cerr << "Subscriber error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

} // namespace rcs
