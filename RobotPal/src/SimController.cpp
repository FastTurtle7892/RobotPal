// /**
//  * @file SimController.cpp
//  * @author Hong Yoon Pyo (cgantro@gmail.com)
//  * @brief 
//  * @version 0.1
//  * @date 2025-11-28
//  * 
//  * @copyright Copyright (c) 2025
//  * 
//  */
// #define GLM_ENABLE_EXPERIMENTAL
// #include "RobotPal/SimController.h"
// #include "RobotPal/Components/Components.h"
// #include "RobotPal/Util/Movement.h"
// #include "RobotPal/Entity.h"
// #include <glm/gtx/norm.hpp> 
// #include <glm/gtx/matrix_decompose.hpp> 

// SimController::SimController(Entity &entity, flecs::world &world)
//     : m_Entity(entity), m_World(world)
// {
// }

// bool SimController::Init()
// {
//     std::cout << ">>> [SimController] Initializing..." << std::endl;
//     std::cout << ">>> Entity is valid: " << (m_Entity.IsValid() ? "Yes" : "No") << std::endl;
//     if (!m_Entity.IsValid()) return false;
//     std::cout << ">>> Entity has Position: " << (m_Entity.Has<Position>() ? "Yes" : "No") << std::endl;
//     std::cout << ">>> Entity has Rotation: " << (m_Entity.Has<Rotation>() ? "Yes" : "No") << std::endl;
//     // if (!m_Entity.Has<Position>() || !m_Entity.Has<Rotation>()) return false;S
//     m_GripperEntity = m_Entity.FindChildByNameRecursive(m_Entity.GetHandle(), "EE");
//     if(m_GripperEntity.IsValid()) {
//         std::cout << ">>> Gripper Entity found: " << m_GripperEntity.GetHandle().name() << std::endl;
//     } else {
//         std::cout << ">>> Gripper Entity not found!" << std::endl;
//     }
//     return true;
// }

// void SimController::Move(const float& v, const float& w)
// {
//     m_TargetV = v;
//     m_TargetW = w;
// }

// void SimController::Update(const float& dt)
// {

//     if (m_currentCooldown > 0.0f)
//     {
//         m_currentCooldown -= dt;
//     }
//     if (!m_Entity.IsValid()) return;
  
//     // 1. 보간 (Soft Start/Stop)
//     const float accel = 5.0f;
//     m_CurrentV += (m_TargetV - m_CurrentV) * accel * dt;
//     m_CurrentW += (m_TargetW - m_CurrentW) * accel * dt;

//     if (std::abs(m_CurrentV) < 0.01f) m_CurrentV = 0.0f;
//     if (std::abs(m_CurrentW) < 0.01f) m_CurrentW = 0.0f;

//     // 2. 데이터 수정 (Controller의 본분)
//     glm::vec3 pos = m_Entity.GetLocalPosition();
//     glm::vec3 rot = m_Entity.GetLocalRotation();


//     // (2) 물리 계산 (Dead Reckoning)
//     // 회전 (Y축) 업데이트
//     MovementMath::CalculateNextStep(pos, rot, m_CurrentV, m_CurrentW, dt);
//     MovementMath::ApplyFriction(m_CurrentV, m_CurrentW);

//     m_Entity.SetLocalPosition(pos);
//     m_Entity.SetLocalRotation(rot);
// }


// // 키를 누르면 그냥 자석처럼 부모(EE)에게 붙도록함



// void SimController::TryGrip()
// {
//     // 쿨타임 체크
//     if(m_currentCooldown > 0.0f) return;

//     // =================================================================
//     // [1] 놓기 (Release)
//     // =================================================================
//     if (m_GripperEntity.Get<GripperLogic>().isGripping) 
//     {
//         Entity &attachedEntity = m_GripperEntity.Get<GripperLogic>().attachedEntity;
//         if (attachedEntity.IsValid()) 
//         {

//             // 1. 현재(그리퍼에 매달린 상태)의 '월드 행렬' 가져오기
//             // Entity 래퍼 내부의 flecs handle을 꺼내서 get()을 호출해야 합니다.
//             // (get_mut은 수정용이므로 읽기만 할 때는 get()이 안전합니다)
//             const TransformMatrix* worldMat = &attachedEntity.GetHandle().get<TransformMatrix, World>();

//             glm::vec3 worldPos(0.0f);
//             glm::vec3 worldRotEuler(0.0f);
//             glm::vec3 worldScale(1.0f);

//             if (worldMat)
//             {
//                 // 2. 월드 행렬 분해 (위치, 회전, 크기 추출)
//                 glm::vec3 scale;
//                 glm::quat rotation;
//                 glm::vec3 translation;
//                 glm::vec3 skew;
//                 glm::vec4 perspective;

//                 glm::decompose(*worldMat, scale, rotation, translation, skew, perspective);

//                 worldPos = translation;
//                 worldRotEuler = glm::eulerAngles(rotation); // 쿼터니언 -> 오일러(Radian)
//                 worldScale = scale;
//             }

//             // 3. [핵심] 부모 관계 끊기
//             // felm_GripperEntity (오타 수정) -> m_GripperEntity
//             // Entity 래퍼에 Remove 함수가 없다면 GetHandle().remove()를 써야 합니다.
//             // 여기서는 Entity 래퍼의 방식(SetParent 등)을 고려해 flecs 원본 함수로 확실하게 끊습니다.
//             attachedEntity.GetHandle().remove(flecs::ChildOf, m_GripperEntity.GetHandle());

//             // 4. [핵심] 추출한 '월드 좌표'를 '내 로컬 좌표'로 덮어쓰기
//             // 함수 이름(Set)을 명시해야 합니다.
//             attachedEntity.SetLocalPosition({worldPos});
//             attachedEntity.SetLocalRotation({worldRotEuler});

//         }

//         attachedEntity = Entity(); // 혹은 flecs::entity::null()에 대응하는 초기화
//         m_GripperEntity.Get<GripperLogic>().isGripping = false;
//         m_currentCooldown = GRIP_COOLDOWN_TIME;
//         return;
//     }

//     // =================================================================
//     // [2] 잡기 (Grab)
//     // =================================================================
    
//     if (!m_GripperEntity.IsValid()) return;

//     // 1. 그리퍼 월드 좌표 (거리 계산용)
//     glm::vec3 gripperPos(0.0f);
//     const TransformMatrix* gripperMat = &m_GripperEntity.GetHandle().get_mut<TransformMatrix, World>();
    
//     if (gripperMat) {
//         gripperPos = glm::vec3((*gripperMat)[3]);
//     }

//     flecs::entity bestTarget = flecs::entity::null();
//     float grabRange = m_GripperEntity.Get<GripperLogic>().grabRange;
//     float minStartDistSq = grabRange * grabRange;

//     // 2. Grabbable 검색
//     auto q = m_Entity.GetHandle().world().query<Grabbable>();
//     q.each([&](flecs::entity e, Grabbable& g) 
//     {
//         if (e == m_Entity.GetHandle() || e == m_GripperEntity.GetHandle()) return;
        
//         // 시스템이 계산한 월드 행렬이 있는 경우에만 거리 계산
//         if (e.has<TransformMatrix, World>())
//         {
//             const TransformMatrix* mat = &e.get<TransformMatrix, World>();
//             glm::vec3 targetPos = glm::vec3((*mat)[3]);
//             float distSq = glm::distance2(gripperPos, targetPos);

//             if (distSq < minStartDistSq)
//             {
//                 minStartDistSq = distSq;
//                 bestTarget = e;
//             }
//         }
//     });

//     // 3. 대상 잡기 처리
//     if (bestTarget.is_valid())
//     {
//         Entity target(bestTarget);

//         // (1) 부모 설정
//         target.SetParent(m_GripperEntity);
        
//         // (2) [핵심] 위치/회전 초기화 (0,0,0)
//         // 이걸 해야 잡는 순간 그리퍼 위치로 "텔레포트"해서 딱 달라붙습니다.
//         // 기존 위치를 유지하면 그리퍼가 움직일 때 오프셋만큼 떨어져서 돕니다.
//         target.SetLocalPosition({glm::vec3(0.0f)}); 
//         target.SetLocalPosition({glm::vec3(0.0f)});
//         // 크기는 유지 (Scale은 건드리지 않거나 1로 리셋)
//         // target.set<Scale>({glm::vec3(1.0f)});

//         m_GripperEntity.Get<GripperLogic>().attachedEntity = bestTarget;
//         m_GripperEntity.Get<GripperLogic>().isGripping = true;
//     }

//     m_currentCooldown = GRIP_COOLDOWN_TIME;
// }

/**
 * @file SimController.cpp
 * @author Hong Yoon Pyo (cgantro@gmail.com)
 * @brief 
 * @version 0.2
 * @date 2025-12-28
 */
#define GLM_ENABLE_EXPERIMENTAL
#include "RobotPal/SimController.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Util/Movement.h"
#include "RobotPal/Entity.h"
#include <glm/gtx/norm.hpp> 
#include <glm/gtx/matrix_decompose.hpp> 
#include <iostream>

// Python 코드 참조 상수
static const float ANGLE_ARM_DOWN = 90.0f;
static const float ANGLE_ARM_UP = 0.0f;
static const float ANGLE_FOREARM_DEFAULT = 0.0f;
static const float ANGLE_GRIP_CLOSE = 40.0f;
static const float ANGLE_GRIP_OPEN = 100.0f;

// 동작 시간 (초)
static const float DURATION_ARM_MOVE = 1.0f; // time.sleep(1)
static const float DURATION_GRIP_ACT = 3.0f; // time.sleep(3)

SimController::SimController(Entity &entity, flecs::world &world)
    : m_Entity(entity), m_World(world)
{
}

bool SimController::Init()
{
    std::cout << ">>> [SimController] Initializing..." << std::endl;
    if (!m_Entity.IsValid()) return false;

    // ---------------------------------------------------------------
    // [수정] ControllerSystemModule에서 확인한 실제 이름으로 변경
    // ---------------------------------------------------------------
    // Servo 2 (Shoulder) -> "Arm1"
    // Servo 3 (Elbow)    -> "Arm2"
    // Servo 4 (Gripper)  -> "EE" (집게 부분)
    
    // 1. 그리퍼 중심점 (패런팅용)
    m_GripperEntity = m_Entity.FindChildByNameRecursive(m_Entity.GetHandle(), "EE");
    
    // 2. 관절 엔티티 연결
    m_Joint2 = m_Entity.FindChildByNameRecursive(m_Entity.GetHandle(), "Arm1"); 
    m_Joint3 = m_Entity.FindChildByNameRecursive(m_Entity.GetHandle(), "Arm2");
    
    // Servo 4가 EE를 움직이도록 설정되어 있으므로 EE를 Joint4로도 사용
    m_Joint4 = m_Entity.FindChildByNameRecursive(m_Entity.GetHandle(), "EE"); 

    // 3. 연결 확인 로그
    if(m_GripperEntity.IsValid()) std::cout << ">>> Target Found: EE (Gripper Center)\n";
    else std::cout << ">>> [ERROR] 'EE' not found!\n";

    if(m_Joint2.IsValid()) std::cout << ">>> Target Found: Arm1 (Servo 2)\n";
    else std::cout << ">>> [ERROR] 'Arm1' not found!\n";

    if(m_Joint3.IsValid()) std::cout << ">>> Target Found: Arm2 (Servo 3)\n";
    else std::cout << ">>> [ERROR] 'Arm2' not found!\n";

    return true;
}

void SimController::Move(const float& v, const float& w)
{
    // 잡기/놓기 동작 중에는 이동 불가하도록 막으려면 아래 주석 해제
    // if (m_State != RobotActionState::IDLE) { m_TargetV = 0; m_TargetW = 0; return; }
    m_TargetV = v;
    m_TargetW = w;
}

void SimController::Update(const float& dt)
{
    if (!m_Entity.IsValid()) return;

    // 1. 쿨타임 감소
    if (m_currentCooldown > 0.0f) m_currentCooldown -= dt;

    // 2. 상태 머신 업데이트 (애니메이션 및 기능 수행)
    UpdateStateMachine(dt);

    // 3. 로봇 본체 이동 (기존 로직)
    const float accel = 5.0f;
    m_CurrentV += (m_TargetV - m_CurrentV) * accel * dt;
    m_CurrentW += (m_TargetW - m_CurrentW) * accel * dt;

    if (std::abs(m_CurrentV) < 0.01f) m_CurrentV = 0.0f;
    if (std::abs(m_CurrentW) < 0.01f) m_CurrentW = 0.0f;

    glm::vec3 pos = m_Entity.GetLocalPosition();
    glm::vec3 rot = m_Entity.GetLocalRotation();

    MovementMath::CalculateNextStep(pos, rot, m_CurrentV, m_CurrentW, dt);
    MovementMath::ApplyFriction(m_CurrentV, m_CurrentW);

    m_Entity.SetLocalPosition(pos);
    m_Entity.SetLocalRotation(rot);
}

// G키 입력 시 호출되는 함수
void SimController::TryGrip()
{
    // 1. 쿨타임이거나, 이미 동작 중이면 무시
    if(m_currentCooldown > 0.0f || m_State != RobotActionState::IDLE) return;

    // 2. 현재 그리퍼 상태 확인
    if (!m_GripperEntity.IsValid()) return;
    bool isGripping = m_GripperEntity.Get<GripperLogic>().isGripping;

    if (isGripping) {
        // [놓기] 시퀀스 시작
        std::cout << ">>> Sequence START: Release Object\n";
        m_State = RobotActionState::RELEASE_DOWN;
        m_StateTimer = 0.0f;
    } else {
        // [잡기] 시퀀스 시작
        // 잡을 대상이 범위 내에 있는지 먼저 체크할 수도 있지만,
        // Python 코드처럼 일단 모션을 수행하고 닫힐 때 체크합니다.
        std::cout << ">>> Sequence START: Grab Object\n";
        m_State = RobotActionState::GRAB_DOWN;
        m_StateTimer = 0.0f;
    }
}

void SimController::UpdateStateMachine(float dt)
{
    if (m_State == RobotActionState::IDLE) return;

    m_StateTimer += dt;

    // ======================================================================
    // [설정] 관절별 회전 방향 (1.0f = 정방향, -1.0f = 반대방향)
    // 동작을 보면서 이 값들만 수정하면 됩니다!
    // ======================================================================
    const float DIR_ARM1 = -1.0f;   // 어깨 (Joint2 / Arm1)
    const float DIR_ARM2 = -1.0f;  // 팔꿈치 (Joint3 / Arm2) -> 예: 반대로 돌리고 싶으면 -1.0f
    const float DIR_GRIP = 1.0f;   // 그리퍼 (Joint4 / EE)
    // ======================================================================

    // 람다 함수 수정: 'sign' 파라미터 추가
    auto LerpAngle = [&](Entity joint, float startDeg, float endDeg, float duration, float sign) {
        if (!joint.IsValid()) return;
        
        float t = glm::clamp(m_StateTimer / duration, 0.0f, 1.0f);
        float currentDeg = glm::mix(startDeg, endDeg, t);
        
        // 위에서 설정한 방향(sign)을 그대로 전달
        SetJointAngle(joint, currentDeg, sign); 
    };

    switch (m_State)
    {
    // ================= [GRAB SEQUENCE] =================
    case RobotActionState::GRAB_DOWN:
        // Arm1(어깨): 0 -> 90
        LerpAngle(m_Joint2, ANGLE_ARM_UP, ANGLE_ARM_DOWN, DURATION_ARM_MOVE, DIR_ARM1);
        
        // Arm2(팔꿈치): 0 -> 0 (필요하면 각도 변경 가능)
        LerpAngle(m_Joint3, ANGLE_FOREARM_DEFAULT, ANGLE_FOREARM_DEFAULT, DURATION_ARM_MOVE, DIR_ARM2);
        
        if (m_StateTimer >= DURATION_ARM_MOVE) {
            m_State = RobotActionState::GRAB_CLOSE;
            m_StateTimer = 0.0f;
            std::cout << ">>> State: GRAB_CLOSE\n";
        }
        break;

    case RobotActionState::GRAB_CLOSE:
        // Gripper: Open -> Close
        // LerpAngle(m_Joint4, ANGLE_GRIP_OPEN, ANGLE_GRIP_CLOSE, DURATION_GRIP_ACT, DIR_GRIP);
        
        // Arm1, Arm2는 내려간 상태 유지 (각도 고정)
        // LerpAngle을 호출해주지 않으면 이전 프레임 각도가 유지되므로 굳이 호출 안 해도 되지만,
        // 확실하게 하려면 마지막 각도로 계속 Set 해주는 것이 좋습니다.
        SetJointAngle(m_Joint2, ANGLE_ARM_DOWN, DIR_ARM1);
        SetJointAngle(m_Joint3, ANGLE_FOREARM_DEFAULT, DIR_ARM2);

        if (m_StateTimer >= DURATION_GRIP_ACT) {
            bool success = PerformGrab(); 
            if (success) std::cout << ">>> Object Grabbed!\n";
            else std::cout << ">>> Grab Failed (Nothing in range)\n";

            m_State = RobotActionState::GRAB_UP;
            m_StateTimer = 0.0f;
            std::cout << ">>> State: GRAB_UP\n";
        }
        break;

    case RobotActionState::GRAB_UP:
        // Arm1: 90 -> 0 (올리기)
        LerpAngle(m_Joint2, ANGLE_ARM_DOWN, ANGLE_ARM_UP, DURATION_ARM_MOVE, DIR_ARM1);
        LerpAngle(m_Joint3, ANGLE_FOREARM_DEFAULT, ANGLE_FOREARM_DEFAULT, DURATION_ARM_MOVE, DIR_ARM2);
        
        // Gripper: 닫힌 상태 유지
        // SetJointAngle(m_Joint4, ANGLE_GRIP_CLOSE, DIR_GRIP);

        if (m_StateTimer >= DURATION_ARM_MOVE) {
            m_State = RobotActionState::IDLE;
            m_currentCooldown = GRIP_COOLDOWN_TIME;
            std::cout << ">>> Sequence FINISHED: IDLE\n";
        }
        break;

    // ================= [RELEASE SEQUENCE] =================
    case RobotActionState::RELEASE_DOWN:
        // Arm1: 0 -> 90 (내리기)
        LerpAngle(m_Joint2, ANGLE_ARM_UP, ANGLE_ARM_DOWN, DURATION_ARM_MOVE, DIR_ARM1);
        LerpAngle(m_Joint3, ANGLE_FOREARM_DEFAULT, ANGLE_FOREARM_DEFAULT, DURATION_ARM_MOVE, DIR_ARM2);
        
        // Gripper: 닫힌 상태 유지
        // SetJointAngle(m_Joint4, ANGLE_GRIP_CLOSE, DIR_GRIP);

        if (m_StateTimer >= DURATION_ARM_MOVE) {
            m_State = RobotActionState::RELEASE_OPEN;
            m_StateTimer = 0.0f;
            std::cout << ">>> State: RELEASE_OPEN\n";
        }
        break;

    case RobotActionState::RELEASE_OPEN:
        // Gripper: Close -> Open
        // LerpAngle(m_Joint4, ANGLE_GRIP_CLOSE, ANGLE_GRIP_OPEN, DURATION_GRIP_ACT, DIR_GRIP);
        
        // Arm: 내려간 상태 유지
        SetJointAngle(m_Joint2, ANGLE_ARM_DOWN, DIR_ARM1);
        SetJointAngle(m_Joint3, ANGLE_FOREARM_DEFAULT, DIR_ARM2);

        if (m_StateTimer >= DURATION_GRIP_ACT) {
            PerformRelease(); 
            std::cout << ">>> Object Released!\n";

            m_State = RobotActionState::RELEASE_UP;
            m_StateTimer = 0.0f;
            std::cout << ">>> State: RELEASE_UP\n";
        }
        break;

    case RobotActionState::RELEASE_UP:
        // Arm1: 90 -> 0 (올리기)
        LerpAngle(m_Joint2, ANGLE_ARM_DOWN, ANGLE_ARM_UP, DURATION_ARM_MOVE, DIR_ARM1);
        LerpAngle(m_Joint3, ANGLE_FOREARM_DEFAULT, ANGLE_FOREARM_DEFAULT, DURATION_ARM_MOVE, DIR_ARM2);
        
        // Gripper: 열린 상태 유지
        // SetJointAngle(m_Joint4, ANGLE_GRIP_OPEN, DIR_GRIP);

        if (m_StateTimer >= DURATION_ARM_MOVE) {
            m_State = RobotActionState::IDLE;
            m_currentCooldown = GRIP_COOLDOWN_TIME;
            std::cout << ">>> Sequence FINISHED: IDLE\n";
        }
        break;
    }
}

void SimController::SetJointAngle(Entity joint, float angleDeg, float axisSign)
{
    // 로컬 X축 회전으로 가정 (모델 축에 따라 변경 필요: glm::vec3(0,0,1) 등)
    if (joint.IsValid()) {
        joint.SetLocalRotation(glm::vec3(glm::radians(angleDeg) * axisSign, 0.0f, 0.0f));
    }
}

// 기존 TryGrip의 [2] 잡기 로직을 분리
bool SimController::PerformGrab()
{
    if (!m_GripperEntity.IsValid()) return false;

    glm::vec3 gripperPos(0.0f);
    const TransformMatrix* gripperMat = &m_GripperEntity.GetHandle().get_mut<TransformMatrix, World>();
    if (gripperMat) {
        gripperPos = glm::vec3((*gripperMat)[3]);
    }

    flecs::entity bestTarget = flecs::entity::null();
    float grabRange = m_GripperEntity.Get<GripperLogic>().grabRange;
    float minStartDistSq = grabRange * grabRange;

    // Grabbable 검색
    auto q = m_Entity.GetHandle().world().query<Grabbable>();
    q.each([&](flecs::entity e, Grabbable& g) 
    {
        if (e == m_Entity.GetHandle() || e == m_GripperEntity.GetHandle()) return;
        
        if (e.has<TransformMatrix, World>())
        {
            const TransformMatrix* mat = &e.get<TransformMatrix, World>();
            glm::vec3 targetPos = glm::vec3((*mat)[3]);
            float distSq = glm::distance2(gripperPos, targetPos);

            if (distSq < minStartDistSq)
            {
                minStartDistSq = distSq;
                bestTarget = e;
            }
        }
    });

    if (bestTarget.is_valid())
    {
        Entity target(bestTarget);
        target.SetParent(m_GripperEntity);
        // 잡는 순간 위치를 0,0,0으로 초기화 (그리퍼에 착 달라붙음)
        target.SetLocalPosition({glm::vec3(0.0f)}); 
        target.SetLocalRotation({glm::vec3(0.0f)});
        
        m_GripperEntity.Get<GripperLogic>().attachedEntity = bestTarget;
        m_GripperEntity.Get<GripperLogic>().isGripping = true;
        return true;
    }
    return false;
}

// 기존 TryGrip의 [1] 놓기 로직을 분리
void SimController::PerformRelease()
{
    if (m_GripperEntity.Get<GripperLogic>().isGripping) 
    {
        Entity &attachedEntity = m_GripperEntity.Get<GripperLogic>().attachedEntity;
        if (attachedEntity.IsValid()) 
        {
            const TransformMatrix* worldMat = &attachedEntity.GetHandle().get<TransformMatrix, World>();
            glm::vec3 worldPos(0.0f);
            glm::vec3 worldRotEuler(0.0f);

            if (worldMat)
            {
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;

                glm::decompose(*worldMat, scale, rotation, translation, skew, perspective);
                worldPos = translation;
                worldRotEuler = glm::eulerAngles(rotation);
            }

            // 부모 관계 끊기
            attachedEntity.GetHandle().remove(flecs::ChildOf, m_GripperEntity.GetHandle());

            // 월드 좌표 유지
            attachedEntity.SetLocalPosition({worldPos});
            attachedEntity.SetLocalRotation({worldRotEuler});
        }

        attachedEntity = Entity(); 
        m_GripperEntity.Get<GripperLogic>().isGripping = false;
    }
}