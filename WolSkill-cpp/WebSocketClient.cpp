#include "WebSocketClient.h"
#include <vector>

#pragma comment(lib, "winhttp.lib")

static constexpr const wchar_t* WS_HOST = L"3rbp1kul8g.execute-api.eu-west-1.amazonaws.com";
static constexpr INTERNET_PORT WS_PORT = INTERNET_DEFAULT_HTTPS_PORT;

WebSocketClient::WebSocketClient() = default;

WebSocketClient::~WebSocketClient() {
    Disconnect();
}

void WebSocketClient::SetCallbacks(MessageCallback onMsg, StateCallback onState) {
    m_onMessage = std::move(onMsg);
    m_onStateChange = std::move(onState);
}

void WebSocketClient::Connect(const std::wstring& awsId, const std::wstring& license) {
    Disconnect();
    m_shouldStop = false;
    m_thread = std::thread(&WebSocketClient::WorkerThread, this, awsId, license);
}

void WebSocketClient::Disconnect() {
    m_shouldStop = true;
    if (m_hWebSocket) {
        WinHttpWebSocketClose(m_hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
    }
    if (m_thread.joinable())
        m_thread.join();
    CloseHandles();
    m_state = State::Disconnected;
    if (m_onStateChange) m_onStateChange(State::Disconnected);
}

void WebSocketClient::Send(const std::string& data) {
    std::lock_guard lock(m_sendMutex);
    if (m_hWebSocket && m_state == State::Connected) {
        WinHttpWebSocketSend(m_hWebSocket,
            WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
            const_cast<void*>(static_cast<const void*>(data.c_str())),
            static_cast<DWORD>(data.size()));
    }
}

void WebSocketClient::CloseHandles() {
    std::lock_guard lock(m_sendMutex);
    if (m_hWebSocket) { WinHttpCloseHandle(m_hWebSocket); m_hWebSocket = nullptr; }
    if (m_hRequest) { WinHttpCloseHandle(m_hRequest); m_hRequest = nullptr; }
    if (m_hConnect) { WinHttpCloseHandle(m_hConnect); m_hConnect = nullptr; }
    if (m_hSession) { WinHttpCloseHandle(m_hSession); m_hSession = nullptr; }
}

void WebSocketClient::WorkerThread(std::wstring awsId, std::wstring license) {
    while (!m_shouldStop) {
        m_state = State::Connecting;
        if (m_onStateChange) m_onStateChange(State::Connecting);

        // Build path with query params
        std::wstring path = L"/prod?awsid=" + awsId + L"&license=" + license;

        m_hSession = WinHttpOpen(L"WolSkill/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
        if (!m_hSession) goto reconnect;

        // Enable TLS 1.2+
        {
            DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
            WinHttpSetOption(m_hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
        }

        m_hConnect = WinHttpConnect(m_hSession, WS_HOST, WS_PORT, 0);
        if (!m_hConnect) goto reconnect;

        m_hRequest = WinHttpOpenRequest(m_hConnect, L"GET", path.c_str(),
            nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE);
        if (!m_hRequest) goto reconnect;

        // Request WebSocket upgrade
        if (!WinHttpSetOption(m_hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0))
            goto reconnect;

        if (!WinHttpSendRequest(m_hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            nullptr, 0, 0, 0))
            goto reconnect;

        if (!WinHttpReceiveResponse(m_hRequest, nullptr))
            goto reconnect;

        m_hWebSocket = WinHttpWebSocketCompleteUpgrade(m_hRequest, 0);
        if (!m_hWebSocket) goto reconnect;

        // Close the request handle; we use the WebSocket handle from now on
        WinHttpCloseHandle(m_hRequest);
        m_hRequest = nullptr;

        m_state = State::Connected;
        if (m_onStateChange) m_onStateChange(State::Connected);

        // Receive loop
        {
            std::vector<BYTE> buf(4096);
            std::string accumulated;

            while (!m_shouldStop) {
                DWORD bytesRead = 0;
                WINHTTP_WEB_SOCKET_BUFFER_TYPE bufType;
                DWORD err = WinHttpWebSocketReceive(m_hWebSocket,
                    buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, &bufType);

                if (err != NO_ERROR) break;

                if (bufType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) break;

                accumulated.append(reinterpret_cast<char*>(buf.data()), bytesRead);

                // Check if this is a complete message (not a fragment)
                if (bufType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
                    bufType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
                    if (m_onMessage) m_onMessage(accumulated);
                    accumulated.clear();
                }
            }
        }

    reconnect:
        CloseHandles();
        m_state = State::Disconnected;
        if (m_onStateChange) m_onStateChange(State::Disconnected);

        // Wait before reconnecting (5 seconds), checking stop flag
        for (int i = 0; i < 50 && !m_shouldStop; ++i)
            Sleep(100);
    }
}
