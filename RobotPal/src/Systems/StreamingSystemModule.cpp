#include "RobotPal/Systems/StreamingSystemModule.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Util/StreamingPipeline.h"

StreamingSystemModule::StreamingSystemModule(flecs::world& world)
    : m_world(world)
{
    m_worker = std::make_unique<StreamingPipeline>(world);

    RegisterObserver(world);
    RegisterSystem(world);
}

StreamingSystemModule::~StreamingSystemModule() = default;

void StreamingSystemModule::RegisterObserver(flecs::world& world)
{
    world.observer<const VideoSender>()
        .event(flecs::OnSet)
        .each([this](flecs::entity, const VideoSender& sender) {
            if (m_worker && sender.url.size()) {
                m_worker->TryConnect(sender.url);
            }
        });
}

void StreamingSystemModule::RegisterSystem(flecs::world& world)
{
    world.system<const Camera, const RenderTarget, const VideoSender>()
        .kind(flecs::PostFrame)
        .rate(2)
        .each([this](flecs::entity, const Camera&, const RenderTarget& rt, const VideoSender&) {
            if(!m_world.get_mut<const NetworkEngineHandle>().instance->IsConnected()) return;
            auto tex = rt.fbo->GetColorAttachment();
            auto raw = tex->GetAsyncData(m_world.get_info()->frame_count_total);
            if (raw.empty()) return;

            int comps =
                tex->GetFormat() == TextureFormat::RGB8 ? 3 :
                tex->GetFormat() == TextureFormat::RGBA8 ? 4 : 0;
            if (!comps) return;

            m_worker->PushFrame(
                std::move(raw),
                tex->GetWidth(),
                tex->GetHeight(),
                comps,
                static_cast<uint32_t>(m_world.get_info()->frame_count_total)
            );
        });
}
