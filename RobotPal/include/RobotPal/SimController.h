// /**
//  * @file SimController.h
//  * @author Hong Yoon Pyo (cgantro@gmail.com)
//  * @brief 
//  * @version 0.1
//  * @date 2025-11-28
//  * 
//  * @copyright Copyright (c) 2025
//  * 
//  */
// #pragma once
// #include "RobotPal/RobotController.h"
// #include "RobotPal/Entity.h"

// class SimController : public IRobotController
// {
// public:
//     SimController(Entity &entity, flecs::world &world);
//     virtual ~SimController() = default;

//     virtual bool Init() override;
//     virtual void Move(const float& v, const float& w) override;
//     virtual void Update(const float& dt) override;
//     void TryGrip();
// private:
//     flecs::world& m_World;
//     Entity m_Entity;
//     Entity m_GripperEntity;
//     float m_currentCooldown = 0.0f;       // 현재 남은 쿨타임
//     const float GRIP_COOLDOWN_TIME = 0.5f; // 0.5초 동안 재입력 방지
//     // 이동 상태
//     float m_TargetV = 0.0f;
//     float m_TargetW = 0.0f;
//     float m_CurrentV = 0.0f;
//     float m_CurrentW = 0.0f;
// };

/**
 * @file SimController.h
 * @author Hong Yoon Pyo (cgantro@gmail.com)
 * @brief RobotPal Simulation Controller with Grab/Release Sequence
 * @version 0.2
 * @date 2025-12-28
 */
#pragma once
#include "RobotPal/RobotController.h"
#include "RobotPal/Entity.h"

// 동작 상태 정의
enum class RobotActionState
{
    IDLE,
    // 잡기 시퀀스
    GRAB_DOWN,      // 팔 내리기 (Servo 2: 90도)
    GRAB_CLOSE,     // 그리퍼 닫기 (Servo 4: 40도) + 패런팅
    GRAB_UP,        // 팔 올리기 (Servo 2: 0도)
    
    // 놓기 시퀀스
    RELEASE_DOWN,   // 팔 내리기 (Servo 2: 90도)
    RELEASE_OPEN,   // 그리퍼 열기 (Servo 4: 100도) + 언패런팅
    RELEASE_UP      // 팔 올리기 (Servo 2: 0도)
};

class SimController : public IRobotController
{
public:
    SimController(Entity &entity, flecs::world &world);
    virtual ~SimController() = default;

    virtual bool Init() override;
    virtual void Move(const float& v, const float& w) override;
    virtual void Update(const float& dt) override;
    
    void TryGrip(); // G키 입력 시 호출

private:
    void UpdateStateMachine(float dt);
    void SetJointAngle(Entity joint, float angleDeg, float axisSign = 1.0f); // 관절 회전 헬퍼
    bool PerformGrab();   // 실제 패런팅 로직
    void PerformRelease(); // 실제 언패런팅 로직

private:
    flecs::world& m_World;
    Entity m_Entity;        // 로봇 본체
    Entity m_GripperEntity; // EE (End Effector)

    // 관절 엔티티 (Servo ID 매핑용)
    Entity m_Joint2; // Arm (Servo 2)
    Entity m_Joint3; // Forearm (Servo 3)
    Entity m_Joint4; // Gripper Finger (Servo 4)

    // 상태 머신 관련
    RobotActionState m_State = RobotActionState::IDLE;
    float m_StateTimer = 0.0f;

    // 쿨타임 및 이동 변수
    float m_currentCooldown = 0.0f;       
    const float GRIP_COOLDOWN_TIME = 0.5f;
    float m_TargetV = 0.0f;
    float m_TargetW = 0.0f;
    float m_CurrentV = 0.0f;
    float m_CurrentW = 0.0f;
};