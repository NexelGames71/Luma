#include "Luma/Platform/Process.h"

#include "Luma/Core/Log.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Luma {

#if defined(_WIN32)

namespace {

std::wstring Widen(const std::string& utf8) {
    if (utf8.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                  static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<usize>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                        static_cast<int>(utf8.size()), wide.data(), len);
    return wide;
}

}  // namespace

std::filesystem::path ExecutablePath() {
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0) return {};
    return std::filesystem::path(std::wstring(buffer, length));
}

bool LaunchDetached(const std::filesystem::path& exe,
                    const std::vector<std::string>& args) {
    std::wstring commandLine = L"\"" + exe.wstring() + L"\"";
    for (const std::string& arg : args) {
        commandLine += L" \"" + Widen(arg) + L"\"";
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    std::vector<wchar_t> mutableCmd(commandLine.begin(), commandLine.end());
    mutableCmd.push_back(L'\0');

    BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
                             DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
                             nullptr, nullptr, &startup, &process);
    if (!ok) {
        LUMA_LOG_ERROR("Process", "CreateProcess failed ({})", GetLastError());
        return false;
    }
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return true;
}

#else  // non-Windows stub (filled in when other platforms are supported)

std::filesystem::path ExecutablePath() { return {}; }
bool LaunchDetached(const std::filesystem::path&,
                    const std::vector<std::string>&) {
    LUMA_LOG_ERROR("Process", "LaunchDetached not implemented on this platform");
    return false;
}

#endif

}  // namespace Luma
