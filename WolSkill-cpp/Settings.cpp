#include "Settings.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.h>

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
    try {
        auto task = winrt::Windows::ApplicationModel::StartupTask::GetAsync(STARTUP_TASK_ID).get();
        return task.State() == winrt::Windows::ApplicationModel::StartupTaskState::Enabled;
    } catch (...) {
        return false;
    }
}

void Settings::SetRunOnStartup(bool enable) {
    try {
        auto task = winrt::Windows::ApplicationModel::StartupTask::GetAsync(STARTUP_TASK_ID).get();
        if (enable) {
            task.RequestEnableAsync().get();
        } else {
            task.Disable();
        }
    } catch (...) {
    }
}
