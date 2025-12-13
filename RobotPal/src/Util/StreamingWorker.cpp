#include "RobotPal/Util/StreamingPipeline.h"

#include "RobotPal/Util/JpegEncoder.h"
#include "RobotPal/Util/bench.h"

#include <algorithm>

static PerfStats perfEncode;

// ---------------------------------------------
// Thread config (Native / Emscripten)
// ---------------------------------------------
struct StreamingThreadConfig {
    unsigned encodeWorkers;
    bool asyncSend;
};

static StreamingThreadConfig GetStreamingThreadConfig() {
#ifdef __EMSCRIPTEN__
    #ifdef __EMSCRIPTEN_PTHREADS__
        return {
            std::max(1u, std::thread::hardware_concurrency() - 1),
            true
        };
    #else
        return {
            1,      // encode worker
            false   // send inline
        };
    #endif
#else
    // Native TCP
    return {
        std::max(1u, std::thread::hardware_concurrency() - 1),
        true
    };
#endif
}

// ---------------------------------------------
// Ctor / Dtor
// ---------------------------------------------
StreamingPipeline::StreamingPipeline(flecs::world& world) {
    auto handle = world.get_mut<const NetworkEngineHandle>();
    m_networkEngine = handle.instance;

    auto cfg = GetStreamingThreadConfig();
    m_isRunning.store(true);

    // Encode workers
    for (unsigned i = 0; i < cfg.encodeWorkers; ++i) {
        m_encodeThreads.emplace_back(
            &StreamingPipeline::EncodeWorkerLoop,
            this
        );
    }

    // Sender thread (optional)
    if (cfg.asyncSend) {
        m_sendThread = std::thread(
            &StreamingPipeline::SendWorkerLoop,
            this
        );
    }
}

StreamingPipeline::~StreamingPipeline() {
    m_isRunning.store(false);

    m_encodeQueueCv.notify_all();
    m_packetQueueCv.notify_all();

    for (auto& t : m_encodeThreads) {
        if (t.joinable())
            t.join();
    }

    if (m_sendThread.joinable())
        m_sendThread.join();
}

// ---------------------------------------------
// Public API
// ---------------------------------------------
void StreamingPipeline::TryConnect(const std::string& url) {
    if (m_networkEngine)
        m_networkEngine->TryConnect(url);
}

void StreamingPipeline::PushFrame(
    std::vector<uint8_t>&& pixels,
    int width,
    int height,
    int components,
    uint32_t frameId
) {
    FrameEncodeJob job;
    job.pixels = std::move(pixels);
    job.width = width;
    job.height = height;
    job.components = components;
    job.frameId = frameId;

    {
        std::lock_guard<std::mutex> lk(m_encodeQueueMutex);
        m_encodeQueue.push(std::move(job));
    }
    m_encodeQueueCv.notify_one();
}

// ---------------------------------------------
// Encode worker
// ---------------------------------------------
void StreamingPipeline::EncodeWorkerLoop() {
    std::unique_ptr<JpegEncoder> jpeg(CreateJpegEncoder());

    while (m_isRunning.load()) {
        FrameEncodeJob job;
        {
            std::unique_lock<std::mutex> lk(m_encodeQueueMutex);
            m_encodeQueueCv.wait(lk, [&] {
                return !m_encodeQueue.empty() || !m_isRunning.load();
            });

            if (!m_isRunning.load() && m_encodeQueue.empty())
                return;

            job = std::move(m_encodeQueue.front());
            m_encodeQueue.pop();
        }

        // RGBA -> RGB if needed
        std::vector<uint8_t> rgb;
        const uint8_t* rgbPtr = nullptr;

        if (job.components == 3) {
            rgbPtr = job.pixels.data();
        } else if (job.components == 4) {
            rgb.resize(job.width * job.height * 3);
            const uint8_t* s = job.pixels.data();
            uint8_t* d = rgb.data();
            for (int i = 0; i < job.width * job.height; ++i) {
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                d += 3;
                s += 4;
            }
            rgbPtr = rgb.data();
        } else {
            continue;
        }

        std::vector<uint8_t> jpegOut;
        double t0 = now_ms();
        bool ok = jpeg->EncodeRGB(
            rgbPtr,
            job.width,
            job.height,
            70,
            jpegOut
        );
        double t1 = now_ms();

        if (!ok)
            continue;

        perfEncode.add(t1 - t0);

        // Packet: [uint32 size][jpeg bytes]
        uint32_t len = static_cast<uint32_t>(jpegOut.size());
        std::vector<uint8_t> packet;
        packet.reserve(4 + jpegOut.size());

        packet.push_back(len & 0xFF);
        packet.push_back((len >> 8) & 0xFF);
        packet.push_back((len >> 16) & 0xFF);
        packet.push_back((len >> 24) & 0xFF);
        packet.insert(packet.end(), jpegOut.begin(), jpegOut.end());

#ifdef __EMSCRIPTEN__
    #ifndef __EMSCRIPTEN_PTHREADS__
        // single-thread WASM: send inline
        if (m_networkEngine)
            m_networkEngine->SendPacket(packet);
        continue;
    #endif
#endif

        {
            std::lock_guard<std::mutex> lk(m_packetQueueMutex);
            m_packetQueue.push(std::move(packet));
        }
        m_packetQueueCv.notify_one();
    }
}

// ---------------------------------------------
// Send worker
// ---------------------------------------------
void StreamingPipeline::SendWorkerLoop() {
    while (m_isRunning.load()) {
        std::vector<uint8_t> packet;
        {
            std::unique_lock<std::mutex> lk(m_packetQueueMutex);
            m_packetQueueCv.wait(lk, [&] {
                return !m_packetQueue.empty() || !m_isRunning.load();
            });

            if (!m_isRunning.load() && m_packetQueue.empty())
                return;

            packet = std::move(m_packetQueue.front());
            m_packetQueue.pop();
        }

        if (m_networkEngine)
            m_networkEngine->SendPacket(packet);
    }
}
