#ifndef __ISYSTEMMODULE_H__
#define __ISYSTEMMODULE_H__
#include <flecs.h>
struct ISystemModule
{
public:
    virtual ~ISystemModule() = default;
    virtual void Register(flecs::world& world) = 0;
};

#endif