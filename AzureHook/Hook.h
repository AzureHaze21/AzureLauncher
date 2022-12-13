#pragma once

#include <string>
#include <optional>
#include <polyhook2/Detour/x86Detour.hpp>
#include <polyhook2/CapstoneDisassembler.hpp>

namespace globals
{
    inline uint64_t oRecv = 0;
    inline uint64_t oSend = 0;
    inline uint64_t oConnect = 0;
    inline uint64_t oCreateProcess = 0;
    inline uint64_t oSetWindowText = 0;
    inline uint64_t oCreateFileW = 0;
    inline uint64_t oCloseHandle = 0;
    inline uint64_t oReadFile = 0;
    inline uint64_t oGetFileInfo = 0;
    inline uint64_t oGetHostName = 0;
    inline uint64_t oWriteFile = 0;

    inline std::optional<PLH::CapstoneDisassembler> dis;
    inline std::optional<PLH::x86Detour> detourRecv;
    inline std::optional<PLH::x86Detour> detourSend;
    inline std::optional<PLH::x86Detour> detourConnect;
    inline std::optional<PLH::x86Detour> detourCreateProcess;
    inline std::optional<PLH::x86Detour> detourSetWindowText;
    inline std::optional<PLH::x86Detour> detourCreateFile;
    inline std::optional<PLH::x86Detour> detourCloseHandle;
    inline std::optional<PLH::x86Detour> detourReadFile;
    inline std::optional<PLH::x86Detour> detourGetFileInfo;
    inline std::optional<PLH::x86Detour> detourGetHostName;
    inline std::optional<PLH::x86Detour> detourWriteFile;

    static std::string login, password;
    static std::wstring modulePath;
    static SOCKET launcherSocket{ INVALID_SOCKET };
    static SOCKET gameSocket{ INVALID_SOCKET };
    static HANDLE loaderHandle = INVALID_HANDLE_VALUE;
    static std::string adapterIp{ "127.0.0.1" };
    static uint16_t launcherPort;
    static std::string proxyIp, proxyLogin, proxyPassword;
    static uint16_t proxyPort{ 26120 };
    static bool bUseMod{ false };
    static bool bSpoofId{ false };
    static std::vector<uint8_t> loaderBytes;
}

extern "C" __declspec(dllexport) void HookChild(const wchar_t* modulePath, const char* adapterIp, uint16_t launcherPort, const char* login, const char* password, bool bSpoofId);
extern "C" __declspec(dllexport) void HookProcess(const wchar_t* modulePath, const char* adapterIp, uint16_t launcherPort, const char*, const char* password, bool bUseMod, int clientArch, bool bSpoofId);
