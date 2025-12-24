/**
 * @file ControllerSystemModule.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-12-22
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#define GLM_ENABLE_EXPERIMENTAL
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
#include <glm/gtx/norm.hpp> 
#include <glm/gtx/matrix_decompose.hpp> 

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
// 잡기 놓기 헬퍼 함수

void ControllerSystemModule::Grip()
{
    // 구현 생략 (필요 시 추가)
    auto &m_GripperEntity = m_ServoEntities[4];
    if (!m_GripperEntity.IsValid())
        return;

    // 1. 그리퍼 월드 위치
    glm::vec3 gripperPos(0.0f);

    if (m_GripperEntity.GetHandle().has<TransformMatrix, World>())
    {
        const TransformMatrix& mat =
            m_GripperEntity.GetHandle().get<TransformMatrix, World>();
        gripperPos = glm::vec3(mat[3]);
    }

    flecs::entity bestTarget = flecs::entity::null();
    float grabRange = m_GripperEntity.Get<GripperLogic>().grabRange;
    float minDistSq = grabRange * grabRange;

    // 2. Grabbable 탐색
    auto q = m_Entity.GetHandle().world().query<Grabbable>();
    q.each([&](flecs::entity e, Grabbable&)
    {
        if (e == m_Entity.GetHandle() ||
            e == m_GripperEntity.GetHandle())
            return;

        if (!e.has<TransformMatrix, World>())
            return;

        const TransformMatrix& mat = e.get<TransformMatrix, World>();
        glm::vec3 targetPos = glm::vec3(mat[3]);

        float distSq = glm::distance2(gripperPos, targetPos);
        if (distSq < minDistSq)
        {
            minDistSq = distSq;
            bestTarget = e;
        }
    });

    // 3. 대상 부착
    if (!bestTarget.is_valid())
        return;

    Entity target(bestTarget);

    target.SetParent(m_GripperEntity);

    // 잡는 순간 정확히 붙도록 초기화
    target.SetLocalPosition(glm::vec3(0.0f));
    target.SetLocalRotation(glm::vec3(0.0f));

    auto& logic = m_GripperEntity.Get<GripperLogic>();
    logic.attachedEntity = target;
    logic.isGripping = true;
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
void ControllerSystemModule::Release()
{
    // 구현 생략 (필요 시 추가)
    auto &m_GripperEntity = m_ServoEntities[4];
    auto& logic = m_GripperEntity.Get<GripperLogic>();
    Entity& attachedEntity = logic.attachedEntity;

    if (!attachedEntity.IsValid())
        return;

    // 1. 현재 월드 행렬 추출
    glm::vec3 worldPos(0.0f);
    glm::vec3 worldRotEuler(0.0f);

    if (attachedEntity.GetHandle().has<TransformMatrix, World>())
    {
        const TransformMatrix& worldMat =
            attachedEntity.GetHandle().get<TransformMatrix, World>();

        glm::vec3 scale, translation, skew;
        glm::quat rotation;
        glm::vec4 perspective;

        glm::decompose(worldMat, scale, rotation, translation, skew, perspective);

        worldPos = translation;
        worldRotEuler = glm::eulerAngles(rotation);
    }

    // 2. 부모 관계 제거
    attachedEntity.GetHandle().remove(
        flecs::ChildOf,
        m_GripperEntity.GetHandle()
    );

    // 3. 월드 좌표 → 로컬 좌표로 재설정
    attachedEntity.SetLocalPosition(worldPos);
    attachedEntity.SetLocalRotation(worldRotEuler);

    // 4. 상태 초기화
    attachedEntity = Entity();
    logic.isGripping = false;
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
            
                // --- [1] 물리 상수 설정 (측정값 기반 보정) ---
                // 0.3 입력 시 2.4cm/s (= 0.024m/s)가 나오도록 설정
                // 계산: 0.024 / 0.3 = 0.08
                const float kLinearGain = 0.08f; 
            
                // 0.3 입력 시 24도/s (= 0.418879 rad/s)가 나오도록 설정
                // 계산: (24 * PI / 180) / 0.3 = 1.39626...
                const float kAngularGain = 1.3963f;
            
                const float kAccel = 5.0f; // 가속도 (반응 속도 조절용)
            
                // --- [2] 목표 속도 계산 ---
                // 선속도: 좌우 모터의 평균값에 비례
                float input_throttle = (leftInput + rightInput) * 0.5f;
                float target_v = input_throttle * kLinearGain;
            
                // 각속도: 좌우 모터의 차이((R-L)/2)에 비례
                // Jetbot 기준: 우측 모터가 더 빠르면 왼쪽(CCW, +각도)으로 회전한다고 가정
                float input_steering = (rightInput - leftInput) * 0.5f;
                float target_w = input_steering * kAngularGain; 
            
                // --- [3] 가감속(Smoothing) 적용 ---
                m_CurrentV += (target_v - m_CurrentV) * kAccel * dt;
                m_CurrentW += (target_w - m_CurrentW) * kAccel * dt;
            
                // 데드존 처리 (노이즈 방지)
                if (std::abs(m_CurrentV) < 0.001f) m_CurrentV = 0.0f;
                if (std::abs(m_CurrentW) < 0.001f) m_CurrentW = 0.0f;
            
                // --- [4] 위치/회전 업데이트 (MovementMath 연동) ---
                glm::vec3 pos = m_Entity.GetLocalPosition();
                glm::vec3 rot = m_Entity.GetLocalRotation();
            
                // 제공하신 MovementMath 헤더 함수 사용
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

                // 그리퍼 로직 처리
                if(id == 4) // Gripper ID
                {
                    auto& logic = joint.Get<GripperLogic>();
                    if (current > 10.0f) // 임계값
                    {
                        if (!logic.isGripping)
                            Grip();
                    }
                    else
                    {
                        if (logic.isGripping)
                            Release();
                    }
                }
            }
        });

}