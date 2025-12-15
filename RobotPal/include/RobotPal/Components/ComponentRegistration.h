#pragma once

namespace flecs {
    class world;
}

namespace RobotPal {
    /**
     * @brief Registers all engine and application components with the flecs world.
     *
     * This function uses flecs::meta to register components and their fields,
     * enabling reflection capabilities for systems like serialization, scripting,
     * and automatic GUI generation.
     *
     * It should be called once at application startup after the world is created.
     *
     * @param world The flecs world instance.
     */
    void register_all_components(flecs::world& world);
}
