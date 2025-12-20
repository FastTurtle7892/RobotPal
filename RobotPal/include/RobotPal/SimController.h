/**
 * @file SimController.h
 * @author Hong Yoon Pyo (cgantro@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2025-11-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#pragma once
#include "RobotPal/RobotController.h"
#include "RobotPal/Entity.h"

class SimController : public IRobotController
{
public:
    SimController(Entity &entity, flecs::world &world);
    virtual ~SimController() = default;

    virtual bool Init() override;
    virtual void Move(const float& v, const float& w) override;
    virtual void Update(const float& dt) override;
    void TryGrip();
private:
    flecs::world& m_World;
    Entity m_Entity;
    Entity m_GripperEntity;
    bool m_isGripping = false;
    bool m_wasPressed = false;      // 키 중복 눌림 방지용
    float m_grabRange = 0.025f;      // 잡는 범위
    

    float m_currentCooldown = 0.0f;       // 현재 남은 쿨타임
    const float GRIP_COOLDOWN_TIME = 0.5f; // 0.5초 동안 재입력 방지
    Entity m_attachedEntity; // 현재 잡고 있는 물체
    // 이동 상태
    float m_TargetV = 0.0f;
    float m_TargetW = 0.0f;
    float m_CurrentV = 0.0f;
    float m_CurrentW = 0.0f;
};