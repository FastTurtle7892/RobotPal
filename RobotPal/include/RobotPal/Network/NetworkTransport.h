#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

struct ConnectionArg
{
    std::string ip;
    int port;
};

#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

class NetworkTransport
{
public:
    using OnRecvCallback = std::function<void(std::vector<uint8_t>&&)>;

    static std::shared_ptr<NetworkTransport> Create(); 
    virtual ~NetworkTransport() = default;

    virtual bool Connect(const std::string& url) = 0;
    virtual void Disconnect() = 0;

    virtual void Send(const std::vector<uint8_t>& data) = 0;
    
    virtual bool IsConnected() const = 0;

    void SetOnRecvCallback(OnRecvCallback callback);

protected:
    void TriggerRecv(std::vector<uint8_t>&& data);

private:
    OnRecvCallback m_OnRecv;
};