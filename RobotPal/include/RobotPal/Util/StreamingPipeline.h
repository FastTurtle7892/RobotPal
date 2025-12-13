#pragma once
#include <vector>
#include <cstdint>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "RobotPal/Network/NetworkEngine.h"

class StreamingPipeline {
public:
    StreamingPipeline(flecs::world& world);
    ~StreamingPipeline();

    void PushFrame(
        std::vector<uint8_t>&& pixels,
        int width,
        int height,
        int components,
        uint32_t frameId
    );

    void TryConnect(const std::string& url);

private:
    struct FrameEncodeJob {
        std::vector<uint8_t> pixels;
        int width;
        int height;
        int components;
        uint32_t frameId;
    };

    void EncodeWorkerLoop();
    void SendWorkerLoop();

private:
    NetworkEngine* m_networkEngine = nullptr;
    std::atomic<bool> m_isRunning{false};

    std::queue<FrameEncodeJob> m_encodeQueue;
    std::mutex m_encodeQueueMutex;
    std::condition_variable m_encodeQueueCv;

    std::queue<std::vector<uint8_t>> m_packetQueue;
    std::mutex m_packetQueueMutex;
    std::condition_variable m_packetQueueCv;

    std::vector<std::thread> m_encodeThreads;
    std::thread m_sendThread;
};
