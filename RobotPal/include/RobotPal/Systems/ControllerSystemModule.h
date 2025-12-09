#ifndef __CONTROLLERSYSTEMMODULEH__
#define __CONTROLLERSYSTEMMODULEH__

#include <flecs.h>
#include "RobotPal/Network/NetworkEngine.h"
#include "RobotPal/Util/DataFormat.h"
#include "RobotPal/Entity.h"

struct ControllerSystemModule
{
public:
    ControllerSystemModule(flecs::world &world);
private:
    void RegisterObserver(flecs::world& world);
    void RegisterSystem(flecs::world& world);

    NetworkEngine* netEngine;
    Entity m_Entity;
    DriveCommand m_LastDriveCmd;
    bool m_HasLastDriveCmd = false;
    
    float m_CurrentV = 0.0f;
    float m_CurrentW = 0.0f;
};

#endif