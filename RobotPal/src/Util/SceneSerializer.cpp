#include "RobotPal/Util/SceneSerializer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace RobotPal {

SceneSerializer::SceneSerializer(flecs::world& world)
    : m_World(world) {}

std::string SceneSerializer::SerializeToString() {
    // flecs에서 생성된 JSON 문자열을 그대로 반환 (줄바꿈/들여쓰기 없음)
    return m_World.to_json().c_str(); 
}

void SceneSerializer::DeserializeFromString(const std::string& data) {
    if (data.empty()) return;
    m_World.from_json(data.c_str());
}

// (Desktop 전용) 파일 저장 구현
void SceneSerializer::save(const std::string& filepath) {
    std::string raw_data = SerializeToString();

    std::ofstream file(filepath); // 텍스트 모드로 열어도 무방함
    if (file.is_open()) {
        file << raw_data; // 그대로 씀
        file.close();
        std::cout << "[SceneSerializer] Saved raw data to " << filepath << std::endl;
    } else {
        std::cerr << "Error [Save]: Unable to open file: " << filepath << std::endl;
    }
}

// (Desktop 전용) 파일 로드 구현
void SceneSerializer::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error [Load]: Unable to open file: " << filepath << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string raw_data = buffer.str();
    file.close();

    DeserializeFromString(raw_data);
    std::cout << "[SceneSerializer] Loaded raw data from " << filepath << std::endl;
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
