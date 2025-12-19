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
};

static StreamingThreadConfig GetStreamingThreadConfig() {
#ifdef __EMSCRIPTEN__
    #ifdef __EMSCRIPTEN_PTHREADS__
        unsigned max_web_workers = 8; // CMake의 8보다 작게 설정 (안전 마진)
        return {std::max(1u, max_web_workers - 2)}; // main + network 제외
    #else
        return {1};     // encode worke  // send inline
    #endif
#else
    // Native TCP
    return {std::max(1u, std::thread::hardware_concurrency() - 3) };
#endif
}


StreamingPipeline::StreamingPipeline(flecs::world& world) {
    auto handle = world.get_mut<const NetworkEngineHandle>();
    m_networkEngine = handle.instance;

    auto cfg = GetStreamingThreadConfig();
    std::cout << cfg.encodeWorkers << " encode workers " << std::endl;
    m_isRunning.store(true);

    // Encode workers
    for (unsigned i = 0; i < cfg.encodeWorkers; ++i) {
        m_encodeThreads.emplace_back(
            &StreamingPipeline::EncodeWorkerLoop,
            this
        );
    }
}

StreamingPipeline::~StreamingPipeline() {
    m_isRunning.store(false);

    m_encodeQueueCv.notify_all(); // Thread 루프 탈출

    for (auto& t : m_encodeThreads) {
        if (t.joinable())
            t.join();
    }
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
    // 일단 이거 수정은 해야할거같음 이름 같은건 일단 내가 핫픽스 해드림 - 준우
    // std::unique_ptr<JpegEncoder> jpeg(CreateJpegEncoder());
    JpegEncoder* jpeg=CreateJpegEncoder();

    std::vector<uint8_t> rgbBuffer;
    std::vector<uint8_t> packetBuffer;

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

        const uint8_t* rgbPtr = nullptr;

        if (job.components == 3) {
            rgbPtr = job.pixels.data();
        } else if (job.components == 4) {
            size_t pixelCount = (size_t)job.width * (size_t)job.height;
            size_t requiredSize = pixelCount * 3;

            if( rgbBuffer.size() < requiredSize )
                rgbBuffer.resize(requiredSize);

            if(rgbBuffer.size() != requiredSize) rgbBuffer.resize(requiredSize);

            const uint8_t* s = job.pixels.data();
            uint8_t* d = rgbBuffer.data();

            for (int i = 0; i < job.width * job.height; ++i) {
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                d += 3;
                s += 4;
            }
            rgbPtr = rgbBuffer.data();
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

        if(m_networkEngine) {
            m_networkEngine->SendPacket(std::move(packet));
        }
    }
}
