#pragma once
#include <Windows.h>

namespace ThemeHelper {
    bool IsSystemDarkMode();
    void InitDarkMode();
    void RefreshDarkMode();
    void ApplyDarkModeToWindow(HWND hWnd);
    HBRUSH GetDialogBackgroundBrush();
    COLORREF GetDialogBackgroundColor();
    COLORREF GetDialogTextColor();
    COLORREF GetEditBackgroundColor();
    void Cleanup();
}
