#ifndef __SCENE_H__
#define __SCENE_H__

#include <flecs.h>
#include "RobotPal/Entity.h"
#include <string>
#include <memory>

namespace RobotPal {
class SceneSerializer;
}

class Scene {
public:
    explicit Scene(flecs::world& world);
    virtual ~Scene();

    virtual void OnEnter() {}
    virtual void OnUpdate(float dt) {}
    virtual void OnExit() {}
    virtual void OnImGuiRender() {}

    Entity CreateEntity(const std::string& name = "");

    void save(const std::string& filepath);
    void load(const std::string& filepath);

    flecs::world& GetWorld();
    flecs::entity GetSceneRoot();

private:
    friend class SceneManager; 

    void InitRoot() {
        m_SceneRoot = m_World.entity("SceneRoot"); 
    }

    void CleanupRoot() {
        if (m_SceneRoot.is_valid()) {
            m_SceneRoot.destruct();
        }
    }

protected:
    flecs::world& m_World;
    flecs::entity m_SceneRoot;
    std::unique_ptr<RobotPal::SceneSerializer> m_Serializer;
};

#endif