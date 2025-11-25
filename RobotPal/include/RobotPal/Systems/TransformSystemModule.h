#ifndef __TRANSFORMSYSTEMGROUP_H__
#define __TRANSFORMSYSTEMGROUP_H__
#include "RobotPal/Systems/ISystemModule.h"
class TransformSystemModule : public ISystemModule {
public:
    TransformSystemModule();
    void Register(flecs::world& world) override;
};

#endif