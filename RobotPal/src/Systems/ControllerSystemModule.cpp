#include "RobotPal/Systems/ControllerSystemModule.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Network/NetworkEngine.h"
#include "RobotPal/Util/JsonParser.h"
#include "RobotPal/Util/Movement.h"
#include "RobotPal/Entity.h"
#include <iostream>
#include <cmath>
#include <glm/glm.hpp>

// 생성자 부분만 수정하면 됩니다.
ControllerSystemModule::ControllerSystemModule(flecs::world &world)
{
    auto handle = world.get_mut<const NetworkEngineHandle>();
    netEngine = handle.instance;

    // ------------------------------------------------------------------
    // [수정] 관절 매핑 및 회전축 재설정
    // ------------------------------------------------------------------
    
    // [ID 1] Base (좌우 회전) -> "Arm0"
    // Y축(0, 1, 0) 기준 회전 (정상)
    m_ServoConfigMap[1] = { "Arm0", glm::vec3(0, -1, 0) }; 

    // [ID 2] Shoulder (어깨) -> "Arm1"
    // X축(1, 0, 0) 기준 회전 (상하 움직임)
    m_ServoConfigMap[2] = { "Arm1", glm::vec3(1, 0, 0) }; 

    // [ID 3] Elbow (팔꿈치) -> "Arm2"
    // X축(1, 0, 0) 기준 회전 (상하 움직임)
    m_ServoConfigMap[3] = { "Arm2", glm::vec3(1, 0, 0) }; 

    // [ID 4] Gripper -> "EE"
    // [설명] 모델이 손가락 분리가 안 된 통짜 모델이라, 집는 동작 대신 
    // 미세하게 회전하거나 움직이지 않게 설정합니다.
    m_ServoConfigMap[4] = { "EE", glm::vec3(0.1, 0, 0) }; 

    // [ID 5] Camera Tilt -> "CamBase" (카메라랑 가장 가까운 관절)
    // [수정] 기존 Y축(좌우) -> X축(1, 0, 0)으로 변경하여 "위아래"로 움직이게 함
    m_ServoConfigMap[5] = { "CamBase", glm::vec3(1, 0, 0) };

    std::cout << "[Controller] Servo Config Updated: CamBase(Axis X), Arm0(Axis Y)\n";

    RegisterObserver(world);
    RegisterSystem(world);
}
// ... 나머지 코드는 그대로 유지 ...

// ... (나머지 함수들은 기존과 동일하게 유지) ...

// [참고] 디버그 함수 및 RegisterObserver, RegisterSystem 등은 
// 이전에 드린 코드 그대로 유지하시면 됩니다.
// (PrintHierarchy 함수가 있어야 나중에 또 이름 확인할 때 편합니다)

void ControllerSystemModule::PrintHierarchy(Entity entity, int depth)
{
    if (!entity.IsValid()) return;
    
    std::string indent(depth * 2, ' ');
    std::cout << "[Hierarchy] " << indent << "- " << entity.GetHandle().name() << "\n";

    entity.GetHandle().children([&](flecs::entity child) {
        PrintHierarchy(Entity(child), depth + 1);
    });
}

void ControllerSystemModule::RegisterObserver(flecs::world &world)
{
    world.observer<const ControllerComponent>()
        .event(flecs::OnSet)
        .each([&](flecs::entity e, const ControllerComponent &)
        {
            m_Entity = Entity(e);
            std::cout << "------------------------------------------------\n";
            std::cout << "[Controller] Robot Loaded: " << m_Entity.GetHandle().name() << "\n";
            PrintHierarchy(m_Entity, 0); 
            std::cout << "------------------------------------------------\n";

            m_ServoEntities.clear();
            m_ServoCurrentAngles.clear();

            for (auto const& [id, config] : m_ServoConfigMap)
            {
                Entity joint = m_Entity.FindChildByNameRecursive(m_Entity.GetHandle(), config.nodeName);
                if (joint.IsValid())
                {
                    m_ServoEntities[id] = joint;
                    m_ServoCurrentAngles[id] = 0.0f;
                    std::cout << "  [OK] ID " << id << " mapped to '" << config.nodeName << "'\n";
                }
                else
                {
                    std::cerr << "  [FAIL] ID " << id << ": Could not find node '" << config.nodeName << "'\n";
                }
            }
        });
}

void ControllerSystemModule::RegisterSystem(flecs::world &world)
{
    world.system<const ControllerComponent>()
        .kind(flecs::OnUpdate)
        .each([&](flecs::entity e, const ControllerComponent &)
        {
            if (!netEngine) return;
            float dt = e.world().delta_time();

            auto packetOpt = netEngine->GetPacket();
            while (packetOpt.has_value())
            {
                DriveCommand driveCmd{};
                ServoCommnad servoCmd{};
                CommandType type = ParseJson(*packetOpt, &driveCmd, &servoCmd);
                std::cout << "[Controller] Received Command Type: " << static_cast<int>(type) << "\n";
                std::cout << "[Controller] Drive Command - Left: " << driveCmd.left << ", Right: " << driveCmd.right << "\n";
                if (type == CommandType::Drive) {
                    m_LastDriveCmd = driveCmd;
                    m_HasLastDriveCmd = true;
                }
                else if (type == CommandType::Servo) {
                    m_LastServoCmds[servoCmd.id] = servoCmd;
                    // 디버깅용: 패킷 수신 확인                
                }
                packetOpt = netEngine->GetPacket();
            }

            // ... (Drive 로직 생략 - 기존 유지) ...
            if (m_HasLastDriveCmd) {
                 // 기존 주행 코드 복사해서 유지하세요
                 float leftInput  = m_LastDriveCmd.left;
                 float rightInput = m_LastDriveCmd.right;
                 // [물리 상수]
                 const float kMetersPerDegree = 0.000433f; 
                 const float kTrackWidth      = 0.7f;      
                 const float kSlipRatio       = 0.65f;     
                 const float kMaxRPM  = 250.0f; 
                 const float kAccel   = 5.0f;    

                 float v_left_mps  = (leftInput  * kMaxRPM) * 6.0f * kMetersPerDegree;
                 float v_right_mps = (rightInput * kMaxRPM) * 6.0f * kMetersPerDegree;

                 float target_v = (v_left_mps + v_right_mps) * 0.5f;
                 float target_w = ((v_right_mps - v_left_mps) / kTrackWidth) * kSlipRatio;

                 m_CurrentV += (target_v - m_CurrentV) * kAccel * dt;
                 m_CurrentW += (target_w - m_CurrentW) * kAccel * dt;

                 if (std::abs(m_CurrentV) < 0.001f) m_CurrentV = 0.0f;
                 if (std::abs(m_CurrentW) < 0.001f) m_CurrentW = 0.0f;

                 glm::vec3 pos = m_Entity.GetLocalPosition();
                 glm::vec3 rot = m_Entity.GetLocalRotation();

                 MovementMath::CalculateNextStep(pos, rot, m_CurrentV, m_CurrentW, dt);
                 m_Entity.SetLocalPosition(pos);
                 m_Entity.SetLocalRotation(rot);
            }

            // Servo 제어 로직
            for (auto& [id, cmd] : m_LastServoCmds)
            {
                if (m_ServoEntities.find(id) == m_ServoEntities.end()) continue;

                Entity joint = m_ServoEntities[id];
                glm::vec3 axis = m_ServoConfigMap[id].axis;

                float target = cmd.angle;
                float current = m_ServoCurrentAngles[id];
                float speed = cmd.speed; 

                if (speed <= 0) current = target;
                else {
                    float step = speed * dt;
                    if (std::abs(target - current) <= step) current = target;
                    else current += (target > current ? 1.0f : -1.0f) * step;
                }
                m_ServoCurrentAngles[id] = current;

                float rad = glm::radians(current);
                // 축 기반 회전 적용
                joint.SetLocalRotation(axis * rad); 
            }
        });
}