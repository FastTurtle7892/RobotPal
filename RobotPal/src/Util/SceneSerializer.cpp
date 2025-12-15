#include "RobotPal/Util/SceneSerializer.h"
#include "RobotPal/Components/Components.h"
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
    auto q = m_World.query_builder<Position, Rotation, Scale>()
        .term_at(0)
        .second<Local>()
        .term_at(1)
        .second<Local>()
        .term_at(2)
        .second<Local>()
        .build();

    // 2. JSON 직렬화 옵션 설정
    flecs::iter_to_json_desc_t desc = ECS_ITER_TO_JSON_INIT;
    desc.serialize_entity_ids = false; // 엔티티 이름 저장 (로드 시 매칭을 위해 필수)
    desc.serialize_values = true;     // x, y, z 값 저장
    desc.serialize_builtin=true;
    desc.serialize_full_paths=true;
    //desc.serialize_type_info=true;
    desc.serialize_inherited=false;
    //desc.serialize_field_info=true;
    desc.dont_serialize_results=false;
    desc.serialize_table=true;
    //desc.serialize_refs=true;

    auto flecsStr=q.iter().to_json(&desc);
    std::string rawData=flecsStr.c_str();
    // std::cout<<"json>"<<rawData<<"<jsonend\n";
    return rawData;
}

void SceneSerializer::DeserializeFromString(const std::string& data) {
    if (data.empty()) return;
        // std::cout<<"jsonfile>"<<data<<"<jsonfileend\n";
    auto q = m_World.query_builder<Position, Rotation, Scale>()
        .term_at(0)
        .second<Local>()
        .term_at(1)
        .second<Local>()
        .term_at(2)
        .second<Local>()
        .build();

    // 로드 수행
    flecs::from_json_desc_t desc = {};
    // 에러 발생 시 원인을 알기 위해 strict 모드를 켜거나 에러 핸들러를 달 수 있습니다. (선택사항)
    desc.strict = true; 

    // 월드에 반영 (이미 있는 엔티티는 업데이트, 없으면 이름 기반으로 생성)
    m_World.from_json(data.c_str(), &desc);
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
