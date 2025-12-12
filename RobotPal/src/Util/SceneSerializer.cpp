#include "RobotPal/Util/SceneSerializer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace RobotPal {

SceneSerializer::SceneSerializer(flecs::world& world)
    : m_World(world) {}

void SceneSerializer::save(const std::string& filepath) {
    flecs::entity root = m_World.lookup(m_SceneRootName);
    if (!root.is_valid()) {
        std::cerr << "Error [Save]: SceneRoot not found." << std::endl;
        return;
    }

    // flecs::query<> q = m_World.query_builder<>()
    //     .with(flecs::ChildOf, root)
    //     .build();
    
    // flecs::iter_to_json_desc_t desc = ECS_ITER_TO_JSON_INIT;
    // desc.serialize_query_info = false;
    // desc.serialize_field_info = false;
    // // desc.dont_serialize_results = true;
    // desc.dont_serialize_results=false;
    // desc.serialize_inherited=true;
    
    std::string json_data = m_World.to_json().c_str();  //q.iter().to_json(&desc);
    //json_data=json::parse(json_data).dump(4);


    std::ofstream file(filepath);
    if (file.is_open()) {
        file << json_data;
        file.close();
        std::cout << "Scene saved to " << filepath << std::endl;
    } else {
        std::cerr << "Error [Save]: Unable to open file for writing: " << filepath << std::endl;
    }
}

void SceneSerializer::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error [Load]: Unable to open file for reading: " << filepath << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_data = buffer.str();
    file.close();

    if (json_data.empty()) {
        std::cerr << "Error [Load]: File is empty or could not be read: " << filepath << std::endl;
        return;
    }

    // clearSceneEntities();

    m_World.from_json(json_data.c_str());

    // reparentSceneEntities(json_data);

    std::cout << "Scene loaded from " << filepath << std::endl;
}

void SceneSerializer::clearSceneEntities() {
    flecs::entity root = m_World.lookup(m_SceneRootName);
    if (root.is_valid()) {
        // root.each([](flecs::entity child) {
        //     child.destruct();
        // });
    }
}

void SceneSerializer::reparentSceneEntities(const std::string& json_data) {
    flecs::entity root = m_World.lookup(m_SceneRootName);
    if (!root.is_valid()) {
        std::cerr << "Error [Reparent]: SceneRoot not found." << std::endl;
        return;
    }

    try {
        auto json = nlohmann::json::parse(json_data);
        if (json.contains("results")) {
            for (const auto& result : json["results"]) {
                if (result.contains("entities")) {
                    for (const auto& entity_name_json : result["entities"]) {
                        if (entity_name_json.is_string()) {
                            std::string name = entity_name_json.get<std::string>();
                            flecs::entity e = m_World.lookup(name.c_str());
                            if (e.is_valid() && !e.has(flecs::ChildOf, root)) {
                                e.child_of(root);
                            }
                        }
                    }
                }
            }
        }
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Error [Reparent]: Failed to parse JSON: " << e.what() << std::endl;
    }
}

}
