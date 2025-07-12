
#include "ImprovedPubSubManager.hxx"
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

using namespace rcs;

std::atomic<bool> running{true};

void handleSigint(int) {
    running = false;
}

int main() {
    std::signal(SIGINT, handleSigint);

    const std::string channel = "my_startup_channel";

    ImprovedPubSubManager& manager = ImprovedPubSubManager::getInstance();

    manager.onMessage([](const std::string& channel, const std::string& msg) {
        std::cout << "[MESSAGE] Channel: " << channel << " | Data: " << msg << std::endl;
    });

    manager.subscribe(channel);
    manager.startConsuming();

    std::this_thread::sleep_for(std::chrono::seconds(1));  // Let subscriber start

    manager.publishOverChannel(channel, "Hello from main!");

    std::cout << "Subscribed to '" << channel << "'. Press Ctrl+C to exit." << std::endl;

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\nShutting down..." << std::endl;
    manager.stopConsuming();
    return 0;
}
