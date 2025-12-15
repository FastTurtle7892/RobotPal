#pragma once

#include <flecs.h>
#include <string>

namespace RobotPal {
class SceneSerializer {
public:
    explicit SceneSerializer(flecs::world& world);

    void save(const std::string& filepath);
    void load(const std::string& filepath);

    std::string SerializeToString(); 
    void DeserializeFromString(const std::string& data);

private:
    void clearSceneEntities();
    void reparentSceneEntities(const std::string& json_data);

    flecs::world& m_World;
    const char* m_SceneRootName = "SceneRoot";
};

}
