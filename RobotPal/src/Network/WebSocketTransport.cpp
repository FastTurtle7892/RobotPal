#include "RobotPal/Network/WebSocketTransport.h"
#ifdef __EMSCRIPTEN__

#include <iostream>
#include <emscripten.h>

WebSocketTransport::WebSocketTransport()
{
}

WebSocketTransport::~WebSocketTransport()
{
    Disconnect();
}

bool WebSocketTransport::Connect(const std::string& url)
{
    if (m_IsConnected) return true;

    if (!emscripten_websocket_is_supported())
    {
        std::cerr << "[WebSocket] WebSockets not supported.\n";
        return false;
    }

    EmscriptenWebSocketCreateAttributes ws_attrs = {
        .url = url.c_str(),
        .protocols = nullptr,
        .createOnMainThread = EM_TRUE
    };

    m_Socket = emscripten_websocket_new(&ws_attrs);
    if (m_Socket <= 0)
    {
        std::cerr << "[WebSocket] Failed to create socket.\n";
        return false;
    }

    // 콜백 등록 (userData로 this 포인터 전달)
    emscripten_websocket_set_onopen_callback(m_Socket, this, OnOpen);
    emscripten_websocket_set_onclose_callback(m_Socket, this, OnClose);
    emscripten_websocket_set_onerror_callback(m_Socket, this, OnError);
    emscripten_websocket_set_onmessage_callback(m_Socket, this, OnMessage);

    std::cout << "[WebSocket] Connecting to " << url << "...\n";
    return true;
}

void WebSocketTransport::Disconnect()
{
    if (m_Socket > 0)
    {
        emscripten_websocket_close(m_Socket, 1000, "Client disconnected");
        emscripten_websocket_delete(m_Socket);
        m_Socket = 0;
    }
    m_IsConnected = false;
}

void WebSocketTransport::Send(const std::vector<uint8_t>& data)
{
    if (m_IsConnected && m_Socket > 0)
    {
        emscripten_websocket_send_binary(m_Socket, (void*)data.data(), data.size());
    }
}

// ------------------------------------------------------------------
// Static Callbacks
// ------------------------------------------------------------------

EM_BOOL WebSocketTransport::OnOpen(int eventType, const EmscriptenWebSocketOpenEvent *e, void *userData)
{
    auto* self = static_cast<WebSocketTransport*>(userData);
    self->m_IsConnected = true;
    std::cout << "[WebSocket] Connected!\n";
    return EM_TRUE;
}

EM_BOOL WebSocketTransport::OnClose(int eventType, const EmscriptenWebSocketCloseEvent *e, void *userData)
{
    auto* self = static_cast<WebSocketTransport*>(userData);
    self->m_IsConnected = false;
    std::cout << "[WebSocket] Closed.\n";
    return EM_TRUE;
}

EM_BOOL WebSocketTransport::OnError(int eventType, const EmscriptenWebSocketErrorEvent *e, void *userData)
{
    std::cerr << "[WebSocket] Error.\n";
    return EM_TRUE;
}

EM_BOOL WebSocketTransport::OnMessage(int eventType, const EmscriptenWebSocketMessageEvent *e, void *userData)
{
    WebSocketTransport* self = static_cast<WebSocketTransport*>(userData);

    if (e->numBytes > 0 && e->data)
    {
        std::vector<uint8_t> packet(e->data, e->data + e->numBytes);

        self->TriggerRecv(std::move(packet));
    }

    return EM_TRUE;
}

#endif // __EMSCRIPTEN__