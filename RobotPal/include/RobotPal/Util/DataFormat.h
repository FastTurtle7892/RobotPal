#ifndef ROBOTPALUTILDATAFORMAT_H
#define ROBOTPALUTILDATAFORMAT_H
#include <vector>

struct WriteContext 
{
    std::vector<uint8_t> buffer;
};

struct Packet {
    std::vector<uint8_t> data;
};

struct DriveCommand
{
    float left;
    float right;
};

struct ServoCommnad
{
    int id;
    float angle;
    float speed;
};

enum class CommandType
{
    Drive,
    Servo,
    Unknown
};

#endif