#ifndef __JSONPARSER_H__
#define __JSONPARSER_H__
#include <vector>
#include <json.hpp>
#include <string>
#include "RobotPal/Util/DataFormat.h"

using json = nlohmann::json;



inline bool ExtractJsonString(const Packet& packet, std::string& outStr)
{
    std::cout << "[JsonParser] Extracting JSON string from packet of size: " << packet.data.size() << std::endl;
    if(packet.data.size() < 4) return false;
    uint32_t size = 0;
    std::memcpy(&size, packet.data.data(), 4);
    std::cout << "[JsonParser] JSON string size: " << size << std::endl;
    if(packet.data.size() < 4 + size) return false;
    outStr.assign(reinterpret_cast<const char*>(packet.data.data() + 4), size);

    return true;
}

inline CommandType ParseJson(const Packet& packet,
                            DriveCommand* DriveCmd,
                            ServoCommnad* ServoCmd){
    std::string jsonString;

    if(!ExtractJsonString(packet, jsonString)){
        return CommandType::Unknown;
    }

    json j;
    try{
        j = json::parse(jsonString);
    }
    catch(...){
        return CommandType::Unknown;
    }

    const std::string type = j.value("type", "");
    std::cout << "[JsonParser] Command Type: " << type << std::endl;
    if(type == "drive"){
        if(DriveCmd){
            DriveCmd->left = j.value("left", 0.0f);
            DriveCmd->right = j.value("right", 0.0f);
        }
        return CommandType::Drive;
    }
    else if(type == "servo"){
        if(ServoCmd){
            ServoCmd->id = j.value("id", 0);
            ServoCmd->angle = j.value("angle", 0.0f);
            ServoCmd->speed = j.value("speed", 0.0f);
        }
        return CommandType::Servo;
    }
    return CommandType::Unknown;
}

#endif // __JSONPARSER_H__