#include "RobotPal/Systems/ControllerSystemModule.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Network/NetworkEngine.h"
#include "RobotPal/Util/JsonParser.h"
#include "RobotPal/Util/Movement.h"
#include "RobotPal/Entity.h"
#include <iostream>
#include <cmath>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

// 생성자
ControllerSystemModule::ControllerSystemModule(flecs::world &world)
{
    auto handle = world.get_mut<const NetworkEngineHandle>();
    netEngine = handle.instance;

    // ------------------------------------------------------------------
    // [설정] 관절 매핑 및 회전축 설정
    // ------------------------------------------------------------------
    
    // [ID 1] Base (좌우 회전) -> "Arm0"
    m_ServoConfigMap[1] = { "Arm0", glm::vec3(0, -1, 0) }; 

    // [ID 2] Shoulder (어깨) -> "Arm1"
    m_ServoConfigMap[2] = { "Arm1", glm::vec3(1, 0, 0) }; 

    // [ID 3] Elbow (팔꿈치) -> "Arm2"
    m_ServoConfigMap[3] = { "Arm2", glm::vec3(1, 0, 0) }; 

    // [ID 4] Gripper -> "EE" (집게 끝)
    m_ServoConfigMap[4] = { "EE", glm::vec3(0.1, 0, 0) }; 

    // [ID 5] Camera Tilt -> "CamBase"
    m_ServoConfigMap[5] = { "CamBase", glm::vec3(1, 0, 0) };

    std::cout << "[Controller] Servo Config Initialized.\n";

    RegisterObserver(world);
    RegisterSystem(world);
}

void ControllerSystemModule::PrintHierarchy(Entity entity, int depth)
{
    if (!entity.IsValid()) return;
    
    std::string indent(depth * 2, ' ');
    std::cout << "[Hierarchy] " << indent << "- " << entity.GetHandle().name() << "\n";

    entity.GetHandle().children([&](flecs::entity child) {
        PrintHierarchy(Entity(child), depth + 1);
    });
}

// ------------------------------------------------------------------
// [수정됨] 초기화 옵저버 (컴포넌트 부착 로직 추가)
// ------------------------------------------------------------------
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

            // 1. 관절 매핑
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

// ------------------------------------------------------------------
// 시스템 등록 (주행, 서보, 그리퍼 로직)
// ------------------------------------------------------------------
void ControllerSystemModule::RegisterSystem(flecs::world &world)
{
    // 1. 주행 및 서보 제어 시스템
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
                
                // 패킷 로그 (필요 시 주석 해제)
                // std::cout << "[Controller] Cmd Type: " << static_cast<int>(type) << "\n";

                if (type == CommandType::Drive) {
                    m_LastDriveCmd = driveCmd;
                    m_HasLastDriveCmd = true;
                }
                else if (type == CommandType::Servo) {
                    m_LastServoCmds[servoCmd.id] = servoCmd;
                }
                packetOpt = netEngine->GetPacket();
            }

            // (1) 주행 로직 (Dead Reckoning)
            if (m_HasLastDriveCmd) {
                 float leftInput  = m_LastDriveCmd.left;
                 float rightInput = m_LastDriveCmd.right;
                 
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

            // (2) 서보 제어 로직
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
                joint.SetLocalRotation(axis * rad); 
            }
        });

    // 2. 그리퍼(집게) 로직 시스템 (G키 토글 + 디버깅)
}