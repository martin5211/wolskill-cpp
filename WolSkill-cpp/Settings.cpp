#include "Settings.h"

bool Settings::Load() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    wchar_t buf[512]{};
    DWORD size;

    size = sizeof(buf);
    if (RegQueryValueExW(hKey, REG_VAL_AWSID, nullptr, nullptr,
        reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS) {
        awsId = buf;
    }

    size = sizeof(buf);
    if (RegQueryValueExW(hKey, REG_VAL_LICENSE, nullptr, nullptr,
        reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS) {
        license = buf;
    }

    RegCloseKey(hKey);
    return true;
}

bool Settings::Save() const {
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, &disp) != ERROR_SUCCESS)
        return false;

    RegSetValueExW(hKey, REG_VAL_AWSID, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(awsId.c_str()),
        static_cast<DWORD>((awsId.size() + 1) * sizeof(wchar_t)));

    RegSetValueExW(hKey, REG_VAL_LICENSE, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(license.c_str()),
        static_cast<DWORD>((license.size() + 1) * sizeof(wchar_t)));

    RegCloseKey(hKey);
    return true;
}

bool Settings::IsValid() const {
    return !awsId.empty() && !license.empty();
}

bool Settings::IsRunOnStartup() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    bool exists = RegQueryValueExW(hKey, RUN_VAL, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return exists;
}

void Settings::SetRunOnStartup(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return;

    if (enable) {
        wchar_t exePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        RegSetValueExW(hKey, RUN_VAL, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(exePath),
            static_cast<DWORD>((wcslen(exePath) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, RUN_VAL);
    }

    RegCloseKey(hKey);
}
