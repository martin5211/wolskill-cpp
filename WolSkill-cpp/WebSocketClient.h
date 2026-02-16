#pragma once
#include <Windows.h>
#include <winhttp.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

class WebSocketClient {
public:
    enum class State { Disconnected, Connecting, Connected };

    using MessageCallback = std::function<void(const std::string& msg)>;
    using StateCallback = std::function<void(State state)>;

    WebSocketClient();
    ~WebSocketClient();

    // Non-copyable
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    void SetCallbacks(MessageCallback onMsg, StateCallback onState);
    void Connect(const std::wstring& awsId, const std::wstring& license);
    void Disconnect();
    void Send(const std::string& data);
    State GetState() const { return m_state.load(); }

private:
    void WorkerThread(std::wstring awsId, std::wstring license);
    void CloseHandles();

    std::atomic<State> m_state{ State::Disconnected };
    std::atomic<bool> m_shouldStop{ false };
    std::thread m_thread;
    std::mutex m_sendMutex;

    HINTERNET m_hSession = nullptr;
    HINTERNET m_hConnect = nullptr;
    HINTERNET m_hRequest = nullptr;
    HINTERNET m_hWebSocket = nullptr;

    MessageCallback m_onMessage;
    StateCallback m_onStateChange;
};
