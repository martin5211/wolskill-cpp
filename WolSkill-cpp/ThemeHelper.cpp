#include "ThemeHelper.h"
#include <dwmapi.h>
#include <uxtheme.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

// DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 10 20H1+)
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Undocumented uxtheme ordinals for dark mode context menus (Windows 10 1903+)
// Ordinal 135: SetPreferredAppMode
// Ordinal 136: FlushMenuThemes
enum class PreferredAppMode { Default = 0, AllowDark = 1, ForceDark = 2, ForceLight = 3 };
using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode);
using fnFlushMenuThemes = void(WINAPI*)();

static fnSetPreferredAppMode g_SetPreferredAppMode = nullptr;
static fnFlushMenuThemes g_FlushMenuThemes = nullptr;

static HBRUSH g_darkBgBrush = nullptr;
static HBRUSH g_lightBgBrush = nullptr;
static HBRUSH g_darkEditBrush = nullptr;

bool ThemeHelper::IsSystemDarkMode() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD val = 1;
    DWORD size = sizeof(val);
    RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
        reinterpret_cast<LPBYTE>(&val), &size);
    RegCloseKey(hKey);
    return val == 0;
}

void ThemeHelper::InitDarkMode() {
    HMODULE hUxTheme = GetModuleHandleW(L"uxtheme.dll");
    if (!hUxTheme)
        hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (!hUxTheme) return;

    g_SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));
    g_FlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136)));

    if (g_SetPreferredAppMode)
        g_SetPreferredAppMode(PreferredAppMode::AllowDark);
    if (g_FlushMenuThemes)
        g_FlushMenuThemes();
}

void ThemeHelper::RefreshDarkMode() {
    if (g_SetPreferredAppMode)
        g_SetPreferredAppMode(PreferredAppMode::AllowDark);
    if (g_FlushMenuThemes)
        g_FlushMenuThemes();
}

void ThemeHelper::ApplyDarkModeToWindow(HWND hWnd) {
    BOOL useDark = IsSystemDarkMode() ? TRUE : FALSE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
}

HBRUSH ThemeHelper::GetDialogBackgroundBrush() {
    if (IsSystemDarkMode()) {
        if (!g_darkBgBrush)
            g_darkBgBrush = CreateSolidBrush(RGB(32, 32, 32));
        return g_darkBgBrush;
    } else {
        if (!g_lightBgBrush)
            g_lightBgBrush = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
        return g_lightBgBrush;
    }
}

COLORREF ThemeHelper::GetDialogBackgroundColor() {
    return IsSystemDarkMode() ? RGB(32, 32, 32) : GetSysColor(COLOR_3DFACE);
}

COLORREF ThemeHelper::GetDialogTextColor() {
    return IsSystemDarkMode() ? RGB(255, 255, 255) : RGB(0, 0, 0);
}

COLORREF ThemeHelper::GetEditBackgroundColor() {
    return IsSystemDarkMode() ? RGB(50, 50, 50) : RGB(255, 255, 255);
}

void ThemeHelper::Cleanup() {
    if (g_darkBgBrush) { DeleteObject(g_darkBgBrush); g_darkBgBrush = nullptr; }
    if (g_lightBgBrush) { DeleteObject(g_lightBgBrush); g_lightBgBrush = nullptr; }
    if (g_darkEditBrush) { DeleteObject(g_darkEditBrush); g_darkEditBrush = nullptr; }
}
