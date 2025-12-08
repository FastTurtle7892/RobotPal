#include "RobotPal/Network/NetworkTransport.h"
#include "RobotPal/Network/TcpNetworkTransport.h"
#include "RobotPal/Network/WebSocketTransport.h"
#include <memory>

std::shared_ptr<NetworkTransport> NetworkTransport::Create()
{
#ifndef __EMSCRIPTEN__
    return std::make_shared<TcpNetworkTransport>();
#else
    return std::make_shared<WebSocketTransport>();
#endif
}

void NetworkTransport::SetOnRecvCallback(OnRecvCallback callback)
{
    m_OnRecv = callback;
}

void NetworkTransport::TriggerRecv(std::vector<uint8_t> &&data)
{
    if (m_OnRecv)
        m_OnRecv(std::move(data));
}
