#pragma once
#include <Windows.h>
#include <string>

class Settings {
public:
    static constexpr const wchar_t* REG_KEY = L"SOFTWARE\\WolSkill";
    static constexpr const wchar_t* REG_VAL_AWSID = L"AwsId";
    static constexpr const wchar_t* REG_VAL_LICENSE = L"License";

    static constexpr const wchar_t* RUN_KEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr const wchar_t* RUN_VAL = L"WolSkill";

    std::wstring awsId;
    std::wstring license;

    bool Load();
    bool Save() const;
    bool IsValid() const;

    static bool IsRunOnStartup();
    static void SetRunOnStartup(bool enable);
};
