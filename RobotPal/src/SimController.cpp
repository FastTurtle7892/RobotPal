/**
 * @file SimController.cpp
 * @author Hong Yoon Pyo (cgantro@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2025-11-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#define GLM_ENABLE_EXPERIMENTAL
#include "RobotPal/SimController.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Util/Movement.h"
#include "RobotPal/Entity.h"
#include <glm/gtx/norm.hpp> // distance2 함수용
#include <glm/gtx/matrix_decompose.hpp> // 행렬 분해용 헤더 필수!

SimController::SimController(Entity &entity, flecs::world &world)
    : m_Entity(entity), m_World(world)
{
}

bool SimController::Init()
{
    std::cout << ">>> [SimController] Initializing..." << std::endl;
    std::cout << ">>> Entity is valid: " << (m_Entity.IsValid() ? "Yes" : "No") << std::endl;
    if (!m_Entity.IsValid()) return false;
    std::cout << ">>> Entity has Position: " << (m_Entity.Has<Position>() ? "Yes" : "No") << std::endl;
    std::cout << ">>> Entity has Rotation: " << (m_Entity.Has<Rotation>() ? "Yes" : "No") << std::endl;
    // if (!m_Entity.Has<Position>() || !m_Entity.Has<Rotation>()) return false;S
    m_GripperEntity = m_Entity.FindChildByNameRecursive(m_Entity.GetHandle(), "EE");
    if(m_GripperEntity.IsValid()) {
        std::cout << ">>> Gripper Entity found: " << m_GripperEntity.GetHandle().name() << std::endl;
    } else {
        std::cout << ">>> Gripper Entity not found!" << std::endl;
    }
    return true;
}

void SimController::Move(const float& v, const float& w)
{
    m_TargetV = v;
    m_TargetW = w;
}

void SimController::Update(const float& dt)
{

    if (m_currentCooldown > 0.0f)
    {
        m_currentCooldown -= dt;
    }
    if (!m_Entity.IsValid()) return;
  
    // 1. 보간 (Soft Start/Stop)
    const float accel = 5.0f;
    m_CurrentV += (m_TargetV - m_CurrentV) * accel * dt;
    m_CurrentW += (m_TargetW - m_CurrentW) * accel * dt;

    if (std::abs(m_CurrentV) < 0.01f) m_CurrentV = 0.0f;
    if (std::abs(m_CurrentW) < 0.01f) m_CurrentW = 0.0f;

    // 2. 데이터 수정 (Controller의 본분)
    glm::vec3 pos = m_Entity.GetLocalPosition();
    glm::vec3 rot = m_Entity.GetLocalRotation();


    // (2) 물리 계산 (Dead Reckoning)
    // 회전 (Y축) 업데이트
    MovementMath::CalculateNextStep(pos, rot, m_CurrentV, m_CurrentW, dt);
    MovementMath::ApplyFriction(m_CurrentV, m_CurrentW);

    m_Entity.SetLocalPosition(pos);
    m_Entity.SetLocalRotation(rot);
}


// 키를 누르면 그냥 자석처럼 부모(EE)에게 붙도록함



void SimController::TryGrip()
{
    // 쿨타임 체크
    if(m_currentCooldown > 0.0f) return;

    // =================================================================
    // [1] 놓기 (Release)
    // =================================================================
    if (m_isGripping) 
    {
        if (m_attachedEntity.IsValid()) 
        {

            // 1. 현재(그리퍼에 매달린 상태)의 '월드 행렬' 가져오기
            // Entity 래퍼 내부의 flecs handle을 꺼내서 get()을 호출해야 합니다.
            // (get_mut은 수정용이므로 읽기만 할 때는 get()이 안전합니다)
            const TransformMatrix* worldMat = &m_attachedEntity.GetHandle().get<TransformMatrix, World>();

            glm::vec3 worldPos(0.0f);
            glm::vec3 worldRotEuler(0.0f);
            glm::vec3 worldScale(1.0f);

            if (worldMat)
            {
                // 2. 월드 행렬 분해 (위치, 회전, 크기 추출)
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;

                glm::decompose(*worldMat, scale, rotation, translation, skew, perspective);

                worldPos = translation;
                worldRotEuler = glm::eulerAngles(rotation); // 쿼터니언 -> 오일러(Radian)
                worldScale = scale;
            }

            // 3. [핵심] 부모 관계 끊기
            // felm_GripperEntity (오타 수정) -> m_GripperEntity
            // Entity 래퍼에 Remove 함수가 없다면 GetHandle().remove()를 써야 합니다.
            // 여기서는 Entity 래퍼의 방식(SetParent 등)을 고려해 flecs 원본 함수로 확실하게 끊습니다.
            m_attachedEntity.GetHandle().remove(flecs::ChildOf, m_GripperEntity.GetHandle());

            // 4. [핵심] 추출한 '월드 좌표'를 '내 로컬 좌표'로 덮어쓰기
            // 함수 이름(Set)을 명시해야 합니다.
            m_attachedEntity.SetLocalPosition({worldPos});
            m_attachedEntity.SetLocalRotation({worldRotEuler});

        }

        m_attachedEntity = Entity(); // 혹은 flecs::entity::null()에 대응하는 초기화
        m_isGripping = false;
        m_currentCooldown = GRIP_COOLDOWN_TIME;
        return;
    }

    // =================================================================
    // [2] 잡기 (Grab)
    // =================================================================
    
    if (!m_GripperEntity.IsValid()) return;

    // 1. 그리퍼 월드 좌표 (거리 계산용)
    glm::vec3 gripperPos(0.0f);
    const TransformMatrix* gripperMat = &m_GripperEntity.GetHandle().get_mut<TransformMatrix, World>();
    
    if (gripperMat) {
        gripperPos = glm::vec3((*gripperMat)[3]);
    }

    flecs::entity bestTarget = flecs::entity::null();
    float minStartDistSq = m_grabRange * m_grabRange;

    // 2. Grabbable 검색
    auto q = m_Entity.GetHandle().world().query<Grabbable>();
    q.each([&](flecs::entity e, Grabbable& g) 
    {
        if (e == m_Entity.GetHandle() || e == m_GripperEntity.GetHandle()) return;
        
        // 시스템이 계산한 월드 행렬이 있는 경우에만 거리 계산
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

    // 3. 대상 잡기 처리
    if (bestTarget.is_valid())
    {
        Entity target(bestTarget);

        // (1) 부모 설정
        target.SetParent(m_GripperEntity);
        
        // (2) [핵심] 위치/회전 초기화 (0,0,0)
        // 이걸 해야 잡는 순간 그리퍼 위치로 "텔레포트"해서 딱 달라붙습니다.
        // 기존 위치를 유지하면 그리퍼가 움직일 때 오프셋만큼 떨어져서 돕니다.
        target.SetLocalPosition({glm::vec3(0.0f)}); 
        target.SetLocalPosition({glm::vec3(0.0f)});
        // 크기는 유지 (Scale은 건드리지 않거나 1로 리셋)
        // target.set<Scale>({glm::vec3(1.0f)});

        m_attachedEntity = bestTarget;
        m_isGripping = true;
    }

    m_currentCooldown = GRIP_COOLDOWN_TIME;
}