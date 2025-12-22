// RobotPal/include/RobotPal/Systems/ControllerSystemModule.h
#ifndef __CONTROLLERSYSTEMMODULEH__
#define __CONTROLLERSYSTEMMODULEH__

#include <flecs.h>
#include <map>
#include <string>
#include <glm/glm.hpp>
#include "RobotPal/Network/NetworkEngine.h"
#include "RobotPal/Util/DataFormat.h"
#include "RobotPal/Entity.h"

// 서보 설정을 위한 구조체
struct ServoConfig {
    std::string nodeName; // GLB 모델 내 노드 이름
    glm::vec3 axis;       // 회전 축 (예: Y축(0,1,0), Z축(0,0,1))
};

struct ControllerSystemModule
{
public:
    ControllerSystemModule(flecs::world &world);
private:
    void RegisterObserver(flecs::world& world);
    void RegisterSystem(flecs::world& world);
    void Grip();
    void Release();
    // 재귀적으로 자식 이름을 모두 출력하는 디버그 함수
    void PrintHierarchy(Entity entity, int depth);

    NetworkEngine* netEngine;
    Entity m_Entity;
    DriveCommand m_LastDriveCmd;
    bool m_HasLastDriveCmd = false;
    float m_CurrentV = 0.0f;
    float m_CurrentW = 0.0f;

    // [수정] 설정(이름+축) 저장 맵
    std::map<int, ServoConfig> m_ServoConfigMap;
    
    // 상태 저장 맵
    std::map<int, Entity> m_ServoEntities;        
    std::map<int, float> m_ServoCurrentAngles;    
    std::map<int, ServoCommnad> m_LastServoCmds;  
};

#endif