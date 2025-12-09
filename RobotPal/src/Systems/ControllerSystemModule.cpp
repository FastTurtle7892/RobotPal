#include "RobotPal/Systems/ControllerSystemModule.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Network/NetworkEngine.h"
#include "RobotPal/Util/JsonParser.h"
#include "RobotPal/Util/Movement.h"
#include "RobotPal/Entity.h"
#include <iostream>
#include <cmath>

ControllerSystemModule::ControllerSystemModule(flecs::world &world)
{
    bool networkEngineFound = false;
    auto handle = world.get_mut<const NetworkEngineHandle>();

    netEngine = handle.instance;

    if (!netEngine)
    {
        std::cerr << "[ControllerSystemModule] check module init order - network engine was not started\n";
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
            // 1) 패킷 읽기
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

            if (!m_HasLastDriveCmd)
                return;

            // -------------------------
            // 2) 물리 기반 움직임 계산
            // -------------------------
            float leftInput  = m_LastDriveCmd.left;
            float rightInput = m_LastDriveCmd.right;

            // [물리 상수]
            const float kMetersPerDegree = 0.000433f; 
            const float kTrackWidth      = 0.7f;      
            const float kSlipRatio       = 0.65f;     
            
            // [속도 튜닝]
            // kMaxRPM: 입력 1.0일 때의 모터 최대 회전수
            // kAccel: 목표 속도 도달 가속도 (클수록 빠릿함)
            const float kMaxRPM  = 250.0f;  // 200 -> 250 상향
            const float kAccel   = 5.0f;    

            // 속도 변환 (Normalized Input -> m/s)
            float v_left_mps  = (leftInput  * kMaxRPM) * 6.0f * kMetersPerDegree;
            float v_right_mps = (rightInput * kMaxRPM) * 6.0f * kMetersPerDegree;

            float target_v = (v_left_mps + v_right_mps) * 0.5f;
            float target_w = ((v_right_mps - v_left_mps) / kTrackWidth) * kSlipRatio;

            float dt = e.world().delta_time(); 

            // 가속도 적용 (Target Velocity로 부드럽게 접근)
            // 이 로직이 가속과 감속(정지)을 모두 수행하므로 별도의 Friction이 필요 없습니다.
            m_CurrentV += (target_v - m_CurrentV) * kAccel * dt;
            m_CurrentW += (target_w - m_CurrentW) * kAccel * dt;

            // Deadzone (아주 미세한 속도는 0으로)
            if (std::abs(m_CurrentV) < 0.001f) m_CurrentV = 0.0f;
            if (std::abs(m_CurrentW) < 0.001f) m_CurrentW = 0.0f;

            glm::vec3 pos = m_Entity.GetLocalPosition();
            glm::vec3 rot = m_Entity.GetLocalRotation();

            MovementMath::CalculateNextStep(pos, rot, m_CurrentV, m_CurrentW, dt);
            
            // [삭제] MovementMath::ApplyFriction(m_CurrentV, m_CurrentW, ...);
            // 이유: 위에서 kAccel을 통해 이미 속도 제어가 되고 있는데, 
            // 여기서 강제로 값을 줄이면 최고 속도가 대폭 깎이고 움직임이 답답해집니다.

            m_Entity.SetLocalPosition(pos);
            m_Entity.SetLocalRotation(rot);
        });
}