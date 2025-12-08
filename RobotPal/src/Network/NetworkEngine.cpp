#include "RobotPal/Network/NetworkEngine.h"
#include <iostream>

NetworkEngine::NetworkEngine(flecs::world &world)
    : m_World(world), m_RecvQueue()
{
    m_Transport = NetworkTransport::Create();

    if (m_Transport)
    {
        m_Transport->SetOnRecvCallback([this](std::vector<uint8_t> &&data)
                                       {
            // 받은 데이터를 엔진의 수신 큐에 저장 (Thread-safe Push)
            m_RecvQueue.Push(std::move(data)); });
    }

    m_World.set<NetworkEngineHandle>(NetworkEngineHandle{this});
    m_World.set<NetworkConnectionState>({ConnectionStatus::Disconnected, "", 0.0f});
}

NetworkEngine::~NetworkEngine()
{
    Disconnect();
}

bool NetworkEngine::TryConnect(const std::string &url)
{
    if (!m_Transport)
        return false;

    m_CurrentUrl = url;
    std::cout << "[NetworkEngine] Connecting to " << url << "...\n";

    // 실제 연결 시도
    return m_Transport->Connect(url);
}
void NetworkEngine::Disconnect()
{
    if (m_Transport)
    {
        m_Transport->Disconnect();
    }

    m_RecvQueue.Clear();
}

bool NetworkEngine::IsConnected() const
{
    return m_Transport && m_Transport->IsConnected();
}

void NetworkEngine::SendPacket(const std::vector<uint8_t> &rawData)
{
    if (m_Transport && m_Transport->IsConnected())
    {
        // WebSocket은 즉시 전송, TCP는 내부 큐에 Push됨 (Non-blocking)
        m_Transport->Send(rawData);
    }
}

std::optional<Packet> NetworkEngine::GetPacket()
{
    return m_RecvQueue.TryPop();
}