#include "RobotPal/Scene.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Util/SceneSerializer.h" // 1. Serializer 헤더 포함
#include <utility>
Scene::Scene(flecs::world& world) : m_World(world) {
    m_Serializer = std::make_unique<RobotPal::SceneSerializer>(m_World);
}
Scene::~Scene() = default;

void Scene::save(const std::string& filepath) {
    if (m_Serializer) {
        m_Serializer->save(filepath);
    }
}

void Scene::load(const std::string& filepath) {
    if (m_Serializer) {
        m_Serializer->load(filepath);
    }
}

Entity Scene::CreateEntity(const std::string &name)
{
    flecs::entity e = m_World.entity(name.c_str());
    e//.add<Position, World>()
     .add<Position, Local>()
     //.add<Rotation, World>()
     .add<Rotation, Local>()
     //.add<Scale, World>()
     .add<Scale, Local>()
     .add<TransformMatrix, World>()
     .add<TransformMatrix, Local>();
    
    if (m_SceneRoot) 
    {
        e.child_of(m_SceneRoot); 
    }
    return Entity(e);
}
flecs::world &Scene::GetWorld()
{
    return m_World;
}

flecs::entity Scene::GetSceneRoot()
{
    return m_SceneRoot;
}
