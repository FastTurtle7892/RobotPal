// StreamingSystemModule_libjpeg.cpp
#include "RobotPal/Systems/StreamingSystemModule.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Network/NetworkEngine.h"
#include "RobotPal/Util/bench.h"

// [추가] libjpeg-turbo 헤더 포함
#include <stdio.h> // jpeglib.h는 stdio.h가 먼저 필요함
extern "C" {
#include <jpeglib.h>
}

// [추가] C2065 'TRUE' 에러 방지 (jpeglib 구버전 호환성)
#ifndef TRUE
#define TRUE 1
#endif
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <iostream>
#include <chrono>
#include <cstring>
#include <algorithm>

static PerfStats perfEncode;

// --------------------------- Job / Queues -------------------------
struct EncodeJob {
    std::vector<uint8_t> raw; // raw pixels, RGB or RGBA
    int w;
    int h;
    int comps; // 3 or 4
    uint32_t frame_id;
};

static std::queue<EncodeJob> g_encodeQueue;
static std::mutex g_encodeMutex;
static std::condition_variable g_encodeCv;

static std::queue<std::vector<uint8_t>> g_sendQueue; // full packet bytes
static std::mutex g_sendMutex;
static std::condition_variable g_sendCv;

static std::atomic<bool> g_workersRunning{false};

// --------------------------- JPEG encode helper -------------------
// Uses libjpeg (jpeg_mem_dest) to emit JPEG into std::vector
static bool EncodeTurboJPEG_mem(const uint8_t* raw, int w, int h, int comps,
                                std::vector<uint8_t>& out, int quality = 70)
{
    if (!raw || w <= 0 || h <= 0 || (comps != 3 && comps != 4)) return false;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    jpeg_mem_dest(&cinfo, &jpegBuf, &jpegSize);

    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = comps == 3 ? 3 : 3; // if RGBA, we will write RGB rows (strip alpha)
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW rowptr[1];
    std::vector<uint8_t> tmpRow;
    tmpRow.resize(static_cast<size_t>(w) * 3);

    if (comps == 3) {
        // direct write rows
        while (cinfo.next_scanline < cinfo.image_height) {
            rowptr[0] = const_cast<JSAMPROW>(reinterpret_cast<const JSAMPLE*>(
                raw + cinfo.next_scanline * w * 3));
            jpeg_write_scanlines(&cinfo, rowptr, 1);
        }
    } else {
        // comps == 4 : strip alpha per row into tmpRow
        while (cinfo.next_scanline < cinfo.image_height) {
            const uint8_t* src = raw + cinfo.next_scanline * w * 4;
            uint8_t* dst = tmpRow.data();
            for (int x = 0; x < w; ++x) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst += 3;
                src += 4;
            }
            rowptr[0] = tmpRow.data();
            jpeg_write_scanlines(&cinfo, rowptr, 1);
        }
    }

    jpeg_finish_compress(&cinfo);

    // move buffer to std::vector
    if (jpegBuf && jpegSize > 0) {
        out.assign(jpegBuf, jpegBuf + jpegSize);
    } else {
        out.clear();
    }

    jpeg_destroy_compress(&cinfo);
    if (jpegBuf) free(jpegBuf);
    return !out.empty();
}

// --------------------------- Worker & Sender threads --------------
static void encodeWorkerThreadFunc()
{
    while (g_workersRunning.load()) {
        EncodeJob job;
        {
            std::unique_lock<std::mutex> lk(g_encodeMutex);
            g_encodeCv.wait(lk, [] { return !g_encodeQueue.empty() || !g_workersRunning.load(); });
            if (!g_workersRunning.load() && g_encodeQueue.empty()) return;
            job = std::move(g_encodeQueue.front());
            g_encodeQueue.pop();
        }

        // Ensure we have RGB data pointer & size
        std::vector<uint8_t> rgb; // hold if need strip alpha
        const uint8_t* rgbPtr = nullptr;
        if (job.comps == 3) {
            rgbPtr = job.raw.data();
        } else {
            // strip alpha into rgb vector
            rgb.resize(static_cast<size_t>(job.w) * job.h * 3);
            const uint8_t* s = job.raw.data();
            uint8_t* d = rgb.data();
            for (int i = 0; i < job.w * job.h; ++i) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                d += 3; s += 4;
            }
            rgbPtr = rgb.data();
        }

        // encode and time
        std::vector<uint8_t> jpeg;
        double t0 = now_ms();
        bool ok = EncodeTurboJPEG_mem(rgbPtr, job.w, job.h, 3, jpeg, 70);
        double t1 = now_ms();
        if (ok) perfEncode.add(t1 - t0);

        if (!ok) {
            // skip or send placeholder
            continue;
        }

        // build packet: [4 byte len][jpeg bytes] (little-endian length)
        uint32_t len = static_cast<uint32_t>(jpeg.size());
        std::vector<uint8_t> packet;
        packet.reserve(4 + jpeg.size());
        packet.push_back(static_cast<uint8_t>(len & 0xFF));
        packet.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        packet.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
        packet.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
        packet.insert(packet.end(), jpeg.begin(), jpeg.end());

        // push to send queue
        {
            std::lock_guard<std::mutex> lk(g_sendMutex);
            g_sendQueue.push(std::move(packet));
        }
        g_sendCv.notify_one();
    }
}

static void senderThreadFunc(NetworkEngine* netEngine)
{
    while (g_workersRunning.load()) {
        std::vector<uint8_t> packet;
        {
            std::unique_lock<std::mutex> lk(g_sendMutex);
            g_sendCv.wait(lk, [] { return !g_sendQueue.empty() || !g_workersRunning.load(); });
            if (!g_workersRunning.load() && g_sendQueue.empty()) return;
            packet = std::move(g_sendQueue.front());
            g_sendQueue.pop();
        }

        // call user's netEngine send (assumed thread-safe or callback based)
        if (netEngine) {
            netEngine->SendPacket(packet);
        }
    }
}

// --------------------------- StreamingSystemModule class --------------------
StreamingSystemModule::StreamingSystemModule(flecs::world& world)
    : m_world(world)
{
    auto handle = world.get_mut<const NetworkEngineHandle>();
    netEngine = handle.instance;

    if (!netEngine) {
        std::cout << "check module init order - network engine was not started\n";
    }

    // start worker pool & sender
    unsigned int hw = std::thread::hardware_concurrency();
    unsigned int workers = hw > 1 ? std::max(1u, hw - 1u) : 1u;
    g_workersRunning = true;
    for (unsigned int i = 0; i < workers; ++i) {
        std::thread(encodeWorkerThreadFunc).detach();
    }
    // sender thread
    m_senderThread = std::thread(senderThreadFunc, netEngine);

    RegisterObserver(world);
    RegisterSystem(world);
}

StreamingSystemModule::~StreamingSystemModule()
{
    // stop threads
    g_workersRunning = false;
    g_encodeCv.notify_all();
    g_sendCv.notify_all();

    // give time for workers to wake & exit; sender thread we join
    if (m_senderThread.joinable()) m_senderThread.join();

    // drain queues (optional)
    {
        std::lock_guard<std::mutex> lk1(g_encodeMutex);
        while (!g_encodeQueue.empty()) g_encodeQueue.pop();
    }
    {
        std::lock_guard<std::mutex> lk2(g_sendMutex);
        while (!g_sendQueue.empty()) g_sendQueue.pop();
    }
}

void StreamingSystemModule::RegisterObserver(flecs::world& world)
{
    world.observer<const VideoSender>()
        .event(flecs::OnSet)
        .each([&](flecs::entity e, const VideoSender& videoCmp) {
            if (netEngine) netEngine->TryConnect(videoCmp.url);
        });
}

void StreamingSystemModule::RegisterSystem(flecs::world& world)
{
    // PostFrame - after rendering
    world.system<const Camera, const RenderTarget, const VideoSender>()
        .kind(flecs::PostFrame)
        .rate(2) // your previous rate; increase/decrease as needed
        .multi_threaded(false)
        .each([this](flecs::entity e, const Camera& cam, const RenderTarget& rt, const VideoSender& sender) {
            if (!netEngine || !netEngine->IsConnected()) return;

            auto tex = rt.fbo->GetColorAttachment();
            uint64_t frame = m_world.get_info()->frame_count_total;

            // GetAsyncData must return a movable std::vector<uint8_t>
            auto raw = tex->GetAsyncData(frame);
            if (raw.empty()) return;

            int width = tex->GetWidth();
            int height = tex->GetHeight();
            int comps = 0;
            if (tex->GetFormat() == TextureFormat::RGB8) comps = 3;
            else if (tex->GetFormat() == TextureFormat::RGBA8) comps = 4;
            else return;

            // Create job and push to queue (move raw to avoid copy)
            EncodeJob job;
            job.w = width;
            job.h = height;
            job.comps = comps;
            job.frame_id = static_cast<uint32_t>(frame);
            job.raw = std::move(raw); // IMPORTANT: assumes GetAsyncData yields moveable vector

            {
                std::lock_guard<std::mutex> lk(g_encodeMutex);
                g_encodeQueue.push(std::move(job));
            }
            g_encodeCv.notify_one();

            // optionally print perf occasionally
            if (perfEncode.count % 120 == 0 && perfEncode.count > 0) {
                perfEncode.print("JPEG encode (libjpeg-turbo)");
            }
        });
}
