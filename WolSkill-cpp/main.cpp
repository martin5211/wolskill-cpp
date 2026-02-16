#include <Windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <strsafe.h>

#include "resource.h"
#include "Settings.h"
#include "WebSocketClient.h"
#include "NetworkInfo.h"
#include "ThemeHelper.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' \
    name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
    processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ---------- Globals ----------
static const wchar_t* g_className = L"WolSkillHiddenWnd";
static const wchar_t* g_appTitle = L"WolSkill";
static const wchar_t* g_mutexName = L"Global\\WolSkillSingleInstance";

static HWND g_hWnd = nullptr;
static HINSTANCE g_hInst = nullptr;
static NOTIFYICONDATAW g_nid{};
static Settings g_settings;
static WebSocketClient g_wsClient;
static bool g_connected = false;
static HICON g_iconConnected = nullptr;
static HICON g_iconDisconnected = nullptr;
static HBRUSH g_editBrush = nullptr;

// ---------- Forward declarations ----------
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);
static void InitTrayIcon(HWND hWnd);
static void RemoveTrayIcon();
static void UpdateTrayIcon();
static void ShowTrayMenu(HWND hWnd);
static void StartConnection();
static void StopConnection();
static void SendMacAddresses();
static void OnWebSocketMessage(const std::string& msg);
static void OnWebSocketStateChanged(WebSocketClient::State state);
static HICON CreateAppIcon(COLORREF color);

// ---------- Entry point ----------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    // Single instance check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, g_mutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    g_hInst = hInstance;

    // Init common controls
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // Register window class
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = g_className;
    RegisterClassExW(&wc);

    // Create hidden message-only window
    g_hWnd = CreateWindowExW(0, g_className, g_appTitle,
        0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance, nullptr);

    if (!g_hWnd) {
        CloseHandle(hMutex);
        return 1;
    }

    // Opt into dark mode for menus before creating any UI
    ThemeHelper::InitDarkMode();

    // Create icons
    g_iconConnected = CreateAppIcon(RGB(0, 180, 80));    // green = connected
    g_iconDisconnected = CreateAppIcon(RGB(180, 40, 40)); // red = disconnected

    // Setup tray icon
    InitTrayIcon(g_hWnd);

    // Load settings and connect
    g_settings.Load();
    if (g_settings.IsValid()) {
        StartConnection();
    }

    // Set up callbacks
    g_wsClient.SetCallbacks(OnWebSocketMessage, OnWebSocketStateChanged);

    // If settings valid, connect
    if (g_settings.IsValid()) {
        StopConnection();
        StartConnection();
    }

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    StopConnection();
    RemoveTrayIcon();
    ThemeHelper::Cleanup();
    if (g_iconConnected) DestroyIcon(g_iconConnected);
    if (g_iconDisconnected) DestroyIcon(g_iconDisconnected);
    if (g_editBrush) DeleteObject(g_editBrush);
    CloseHandle(hMutex);

    return static_cast<int>(msg.wParam);
}

// ---------- Create a simple colored circle icon ----------
static HICON CreateAppIcon(COLORREF color) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmColor = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, nullptr);

    HBITMAP hOld = static_cast<HBITMAP>(SelectObject(hdcMem, hbmColor));

    // Fill transparent background
    RECT rc{ 0, 0, cx, cy };
    HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdcMem, &rc, hBrush);
    DeleteObject(hBrush);

    // Draw filled circle
    HBRUSH hCircleBrush = CreateSolidBrush(color);
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hdcMem, hCircleBrush));
    HPEN hOldPen = static_cast<HPEN>(SelectObject(hdcMem, hPen));
    Ellipse(hdcMem, 1, 1, cx - 1, cy - 1);
    SelectObject(hdcMem, hOldBrush);
    SelectObject(hdcMem, hOldPen);
    DeleteObject(hCircleBrush);
    DeleteObject(hPen);

    // Create mask: black where the circle is, white elsewhere
    SelectObject(hdcMem, hbmMask);
    HBRUSH hWhite = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    FillRect(hdcMem, &rc, hWhite);
    HBRUSH hBlack = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    HPEN hBlackPen = static_cast<HPEN>(GetStockObject(BLACK_PEN));
    SelectObject(hdcMem, hBlack);
    SelectObject(hdcMem, hBlackPen);
    Ellipse(hdcMem, 1, 1, cx - 1, cy - 1);

    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmMask = hbmMask;
    ii.hbmColor = hbmColor;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    return hIcon;
}

// ---------- Tray icon ----------
static void InitTrayIcon(HWND hWnd) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_iconDisconnected;
    StringCchCopyW(g_nid.szTip, _countof(g_nid.szTip), L"WolSkill - Disconnected");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Use version 4 for better balloon/interaction support
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
}

static void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static void UpdateTrayIcon() {
    g_nid.hIcon = g_connected ? g_iconConnected : g_iconDisconnected;
    StringCchCopyW(g_nid.szTip, _countof(g_nid.szTip),
        g_connected ? L"WolSkill - Connected" : L"WolSkill - Disconnected");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void ShowTrayMenu(HWND hWnd) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Status item (disabled, just informational)
    UINT statusFlags = MF_STRING | MF_GRAYED;
    AppendMenuW(hMenu, statusFlags, IDM_STATUS,
        g_connected ? L"\x2705  Connected" : L"\x274C  Disconnected");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"Settings...");
    AppendMenuW(hMenu, Settings::IsRunOnStartup() ? (MF_STRING | MF_CHECKED) : MF_STRING,
        IDM_STARTUP, L"Run on startup");
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    // Required for tray menus to dismiss properly
    SetForegroundWindow(hWnd);
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

// ---------- Connection management ----------
static void StartConnection() {
    if (!g_settings.IsValid()) return;
    g_wsClient.SetCallbacks(OnWebSocketMessage, OnWebSocketStateChanged);
    g_wsClient.Connect(g_settings.awsId, g_settings.license);
}

static void StopConnection() {
    KillTimer(g_hWnd, IDT_HEARTBEAT);
    KillTimer(g_hWnd, IDT_MACSEND);
    g_wsClient.Disconnect();
}

static void SendMacAddresses() {
    std::string json = GetAdaptersJson();
    g_wsClient.Send(json);
}

// ---------- WebSocket callbacks (called from worker thread) ----------
static void OnWebSocketMessage(const std::string& msg) {
    // Parse simple JSON to check for "pong" value
    // The server sends: {"value":"pong"} or {"value":"XX-XX-XX-XX-XX-XX"}
    std::string value;
    auto pos = msg.find("\"value\"");
    if (pos != std::string::npos) {
        auto colon = msg.find(':', pos);
        if (colon != std::string::npos) {
            auto q1 = msg.find('"', colon);
            if (q1 != std::string::npos) {
                auto q2 = msg.find('"', q1 + 1);
                if (q2 != std::string::npos) {
                    value = msg.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }
    }

    if (value == "pong") {
        // Reset heartbeat timer and schedule MAC send
        PostMessageW(g_hWnd, WM_WS_STATUS_CHANGED, 1, 0); // 1 = pong received
    } else if (!value.empty()) {
        // Check if the value matches any local MAC address
        auto macs = GetLocalMacAddresses();
        for (auto& mac : macs) {
            if (value == mac) {
                // Trigger shutdown (matching the Node.js behavior)
                HANDLE hToken;
                if (OpenProcessToken(GetCurrentProcess(),
                    TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
                    TOKEN_PRIVILEGES tp;
                    LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid);
                    tp.PrivilegeCount = 1;
                    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                    AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr);
                    CloseHandle(hToken);
                }
                ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_FLAG_PLANNED);
                break;
            }
        }
    }
}

static void OnWebSocketStateChanged(WebSocketClient::State state) {
    bool connected = (state == WebSocketClient::State::Connected);
    if (connected) {
        // Connected: send MAC addresses immediately and start timers
        PostMessageW(g_hWnd, WM_WS_STATUS_CHANGED, 2, 0); // 2 = connected
    } else {
        PostMessageW(g_hWnd, WM_WS_STATUS_CHANGED, 0, 0); // 0 = disconnected
    }
}

// ---------- Settings dialog ----------
static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        ThemeHelper::ApplyDarkModeToWindow(hDlg);
        SetDlgItemTextW(hDlg, IDC_EDIT_AWSID, g_settings.awsId.c_str());
        SetDlgItemTextW(hDlg, IDC_EDIT_LICENSE, g_settings.license.c_str());
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return reinterpret_cast<INT_PTR>(ThemeHelper::GetDialogBackgroundBrush());

    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, ThemeHelper::GetDialogTextColor());
        SetBkColor(hdc, ThemeHelper::GetDialogBackgroundColor());
        return reinterpret_cast<INT_PTR>(ThemeHelper::GetDialogBackgroundBrush());
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, ThemeHelper::GetDialogTextColor());
        COLORREF editBg = ThemeHelper::GetEditBackgroundColor();
        SetBkColor(hdc, editBg);
        if (g_editBrush) DeleteObject(g_editBrush);
        g_editBrush = CreateSolidBrush(editBg);
        return reinterpret_cast<INT_PTR>(g_editBrush);
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_OK: {
            wchar_t buf[512]{};
            GetDlgItemTextW(hDlg, IDC_EDIT_AWSID, buf, _countof(buf));
            g_settings.awsId = buf;
            GetDlgItemTextW(hDlg, IDC_EDIT_LICENSE, buf, _countof(buf));
            g_settings.license = buf;
            g_settings.Save();

            // Reconnect with new settings
            StopConnection();
            StartConnection();

            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDC_BTN_CANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// ---------- Window procedure ----------
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        // NOTIFYICON_VERSION_4: lParam low word = event, high word = icon id
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu(hWnd);
            break;
        case WM_LBUTTONDBLCLK:
            DialogBoxW(g_hInst, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, SettingsDlgProc);
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_SETTINGS:
            DialogBoxW(g_hInst, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, SettingsDlgProc);
            break;
        case IDM_STARTUP:
            Settings::SetRunOnStartup(!Settings::IsRunOnStartup());
            break;
        case IDM_EXIT:
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_WS_STATUS_CHANGED:
        switch (wParam) {
        case 0: // Disconnected
            g_connected = false;
            KillTimer(hWnd, IDT_HEARTBEAT);
            KillTimer(hWnd, IDT_MACSEND);
            UpdateTrayIcon();
            break;
        case 1: // Pong received - reset heartbeat, schedule MAC send
            g_connected = true;
            SetTimer(hWnd, IDT_HEARTBEAT, 40000, nullptr);
            KillTimer(hWnd, IDT_MACSEND);
            SetTimer(hWnd, IDT_MACSEND, 30000, nullptr);
            UpdateTrayIcon();
            break;
        case 2: // Connected - send MACs immediately, start heartbeat
            g_connected = true;
            SendMacAddresses();
            SetTimer(hWnd, IDT_HEARTBEAT, 40000, nullptr);
            SetTimer(hWnd, IDT_MACSEND, 30000, nullptr);
            UpdateTrayIcon();
            break;
        }
        return 0;

    case WM_TIMER:
        switch (wParam) {
        case IDT_HEARTBEAT:
            // Heartbeat timeout: no pong received in 40s
            // The WebSocket client auto-reconnects, so just update status
            g_connected = false;
            KillTimer(hWnd, IDT_HEARTBEAT);
            KillTimer(hWnd, IDT_MACSEND);
            UpdateTrayIcon();
            break;
        case IDT_MACSEND:
            SendMacAddresses();
            break;
        }
        return 0;

    case WM_SETTINGCHANGE:
        // Detect theme change
        if (lParam && wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
            ThemeHelper::Cleanup();
            ThemeHelper::RefreshDarkMode();
            UpdateTrayIcon();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
