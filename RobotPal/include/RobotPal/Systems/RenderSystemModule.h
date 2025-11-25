#ifndef __RENDERSYSTEMMODULE_H__
#define __RENDERSYSTEMMODULE_H__
#include "RobotPal/Systems/ISystemModule.h"
class RenderSystemModule : public ISystemModule {
public:
    RenderSystemModule();

    void Register(flecs::world& world) override;
};

#endif