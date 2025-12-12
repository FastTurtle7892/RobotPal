#include "RobotPal/Components/ComponentRegistration.h"
#include "RobotPal/Components/Components.h"
#include "flecs.h"

// Note: This file assumes that flecs::meta is included/enabled.
// If not, you might need to include <flecs/addons/meta.h>

namespace RobotPal {

void register_all_components(flecs::world& world) {
    // -- Register Primitive/Wrapper Types --

    // // Register ResourceID to expose its 'value' field
    // world.component<ResourceID>()
    //     .member<uint64_t>("value");

    // // Register Entity to expose its underlying handle ID
    // // flecs::entity::id() returns a flecs::id_t, which is a uint64_t
    // world.component<Entity>()
    //     .member<flecs::id_t>("m_EntityHandle");

    // -- Register GLM Math Types --
    // These are used by many transform components.
    
    world.component<glm::vec3>()
    .member<float>("x")
    .member<float>("y")
    .member<float>("z");

    world.component<glm::vec4>()
    .member<float>("x")
    .member<float>("y")
    .member<float>("z")
    .member<float>("w");
    
    // A mat4 is an array of 4 vec4s. Flecs meta can handle this.
    // We register it as a C-style array member.
    world.component<glm::mat4>()
    .member<glm::vec4>("col", 4);
    
    
    // -- Register Core Components --
    
    // Transform components that inherit from glm types.
    // Flecs meta will use the registered base type (e.g., glm::vec3)
    // to understand their structure.
    world.component<Position>()
        .member<float>("x")
        .member<float>("y")
        .member<float>("z");
    world.component<Rotation>()
        .member<float>("x")
        .member<float>("y")
        .member<float>("z");
        
    world.component<Scale>()
        .member<float>("x")
        .member<float>("y")
        .member<float>("z");

    world.component<TransformMatrix>()
        .member<glm::vec4>("col", 4);

    // Tag components for local vs. world space transforms
    world.component<Local>();
    world.component<World>();


    // // Rendering components
    // world.component<MeshFilter>()
    //     .member<ResourceID>("meshID");

    // world.component<MeshRenderer>()
    //     .member<std::vector<ResourceID>>("materials") // flecs has built-in support for std::vector
    //     .member<bool>("castShadows")
    //     .member<bool>("receiveShadows");

    // // Camera components
    // world.component<Camera>()
    //     .member<float>("fov")
    //     .member<float>("nearPlane")
    //     .member<float>("farPlane")
    //     .member<bool>("useFisheye");

    // world.component<RenderTarget>()
    //     .member<std::shared_ptr<Framebuffer>>("fbo"); // Note: shared_ptr might have limitations with meta

    // // Network components
    // world.component<VideoSender>()
    //     .member<std::string>("url"); // flecs has built-in support for std::string

    // //world.component<ControllerComponent>()
    // //    .member<Entity>("entity");
}

} // namespace RobotPal
