#include "RobotPal/Systems/ControllerSystemModule.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Network/NetworkEngine.h"
#include "RobotPal/Util/JsonParser.h"
#include "RobotPal/Util/Movement.h"
#include "RobotPal/Entity.h"
#include <iostream>

ControllerSystemModule::ControllerSystemModule(flecs::world &world)
{
    bool networkEngineFound = false;
    auto handle = world.get_mut<const NetworkEngineHandle>();

    netEngine = handle.instance;

    if (!netEngine)
    {
        std::cerr << "[ControllerSystemModule]check module init order - network engine was not started\n";
    }

    std::cout << "[ControllerSystemModule] Registering Observer for ControllerComponent\n";
    RegisterObserver(world);
    std::cout << "[ControllerSystemModule] Registering System for ControllerComponent\n";
    RegisterSystem(world);
}

void ControllerSystemModule::RegisterObserver(flecs::world &world)
{

    world.observer<const ControllerComponent>()
        .event(flecs::OnSet)
        .each([&](flecs::entity e, const ControllerComponent &Concmp)
              {
                m_Entity = Entity(e);
                std::cout << "[ControllerSystemModule] ControllerComponent set for entity: " << m_Entity.GetHandle().name() << "\n";
            });
}

void ControllerSystemModule::RegisterSystem(flecs::world &world)
{
    world.system<const ControllerComponent>()
        .kind(flecs::OnUpdate)
        .each([&](flecs::entity e, const ControllerComponent /**/)
        {
            if (!netEngine || !netEngine->IsConnected())
                return;

            // -------------------------
            // 1) 패킷 읽기 (명령 저장만)
            // -------------------------
            auto packetOpt = netEngine->GetPacket();
            while (packetOpt.has_value())
            {
                DriveCommand driveCmd{};
                ServoCommnad servoCmd{};
                CommandType cmdType = ParseJson(*packetOpt, &driveCmd, &servoCmd);

                if (cmdType == CommandType::Drive)
                {
                    m_LastDriveCmd = driveCmd;
                    m_HasLastDriveCmd = true;
                }
                else if (cmdType == CommandType::Servo)
                {
                    // TODO: servo
                }

                packetOpt = netEngine->GetPacket();
            }

            // -------------------------
            // 2) drive 명령이 한 번도 없음 → 아무것도 하지 않음
            // -------------------------
            if (!m_HasLastDriveCmd)
                return;

            // -------------------------
            // 3) 마지막 명령 기반으로 계속 움직임 유지
            // -------------------------
            float left  = m_LastDriveCmd.left;
            float right = m_LastDriveCmd.right;

            const float track_width      = 0.7f;
            const float slip_ratio       = 0.65f;
            const float traction_factor  = 0.9f;
            const float accel            = 4.0f;

            float dt = e.world().delta_time();

            float v = (left + right) * 0.5f * traction_factor;
            float w = ((right - left) / track_width) * slip_ratio;

            m_CurrentV += (v - m_CurrentV) * accel * dt;
            m_CurrentW += (w - m_CurrentW) * accel * dt;

            if (std::abs(m_CurrentV) < 0.01f) m_CurrentV = 0.0f;
            if (std::abs(m_CurrentW) < 0.01f) m_CurrentW = 0.0f;

            glm::vec3 pos = m_Entity.GetLocalPosition();
            glm::vec3 rot = m_Entity.GetLocalRotation();

            MovementMath::CalculateNextStep(pos, rot, m_CurrentV, m_CurrentW, dt);
            MovementMath::ApplyFriction(m_CurrentV, m_CurrentW, 0.95f);

            m_Entity.SetLocalPosition(pos);
            m_Entity.SetLocalRotation(rot);
        });
}
