#include "pch.h"

static bool StartEmulator(int nRetries = 5)
{
    CreateThread(0, 0, [](LPVOID lpData) -> DWORD {
        boost::asio::io_context io_context;
        LauncherEmulator emulator(io_context, globals::launcherPort, globals::login, globals::password, globals::adapterIp);
        return io_context.run();
        }, nullptr, 0, 0);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN launcherAddr;
    launcherAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    launcherAddr.sin_family = AF_INET;
    launcherAddr.sin_port = htons(globals::launcherPort);

    for (int i = 0; i < nRetries; i++)
    {
        if (!connect(s, (sockaddr*)&launcherAddr, sizeof(launcherAddr)))
        {
            closesocket(s);
            break;
        }
        else if (i == nRetries - 1)
        {
            MessageBoxA(0, "Failed to connect to launcher", "error", 0);
            return false;
        }
        Sleep(1000);
    }

    std::cout << "[+] Connection to launcher ok" << std::endl;

    return true;
}

BOOL WINAPI hkCreateProcess(
    LPCWSTR               lpApplicationName,
    LPWSTR                lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCWSTR               lpCurrentDirectory,
    LPSTARTUPINFOW        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
)
{
    std::wcout << L"[CreateProcess] " << (lpApplicationName ? lpApplicationName : L"") << " " << (lpCommandLine ? lpCommandLine : L"") << std::endl;

    LPWSTR cmdLine = (lpCommandLine);

    auto success = PLH::FnCast(globals::oCreateProcess, &CreateProcessW)(
        lpApplicationName,
        cmdLine,
        lpProcessAttributes,
        lpThreadAttributes,
        bInheritHandles,
        dwCreationFlags | CREATE_SUSPENDED,
        lpEnvironment,
        lpCurrentDirectory,
        lpStartupInfo,
        lpProcessInformation
        );

    blackbone::Process childProc;

    childProc.Attach(lpProcessInformation->dwProcessId);
    childProc.EnsureInit();

    if (lpCommandLine != nullptr /*&& (std::wstring(lpCommandLine).find(L"--utility-sub-type=network.mojom.NetworkService") != std::wstring::npos)*/)
    {
        if (auto [success, mod] = childProc.mmap().MapImage(globals::modulePath); mod)
        {
            if (auto [success, fn] = childProc.modules().GetExport(mod.value(), "HookChild"); fn)
            {
                blackbone::RemoteFunction<decltype(&HookChild)> pFN(childProc, blackbone::ptr_t(fn->procAddress));

                pFN.Call({ globals::modulePath.data(), globals::adapterIp.data(), globals::launcherPort, globals::login.data(), globals::password.data(), globals::bSpoofId});
            }
        }
    }

    childProc.Resume();

    return success;
}

BOOL WINAPI hkGetFileAttributesExW(
    LPCWSTR                 lpFileName,
    GET_FILEEX_INFO_LEVELS fInfoLevelId,
    LPVOID                 lpFileInformation
)
{
    auto ret = PLH::FnCast(globals::oGetFileInfo, &GetFileAttributesExW)(lpFileName, fInfoLevelId, lpFileInformation);

    if (globals::bUseMod && lpFileName && (Utils::EndsWith(std::wstring(lpFileName), L"\\loader.swf") || Utils::EndsWith(std::wstring(lpFileName), L"/loader.swf")))
    {
        ((_WIN32_FILE_ATTRIBUTE_DATA*)lpFileInformation)->nFileSizeLow = std::size(globals::loaderBytes);
    }

    return ret;
}

int WINAPI hkGetHostName(char* lpBuffer, int nSize)
{
    nSize = nSize > 32 ? 32 : nSize;

    auto spoofedName = Utils::RandomString(nSize > 32 ? 32 : nSize);

    std::memcpy(lpBuffer, spoofedName.data(), nSize);

    return 0;
}

BOOL WINAPI hkSetWindowText(HWND hwnd, LPCWSTR txt)
{
    std::wstring newTitle = std::format(L"{} (pid: {}, network: {}, launcher port: {})", (wchar_t*)txt, GetCurrentProcessId(), std::wstring(globals::adapterIp.begin(), globals::adapterIp.end()).data(), globals::launcherPort);

    return PLH::FnCast(globals::oSetWindowText, &SetWindowTextW)(hwnd, newTitle.data());
}


BOOL WINAPI hkReadFile(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
)
{
    if (globals::bUseMod && hFile == globals::loaderHandle)
    {
        static std::size_t i = 0;

        if (i <= std::size(globals::loaderBytes))
        {
            nNumberOfBytesToRead = i + nNumberOfBytesToRead > std::size(globals::loaderBytes) ? std::size(globals::loaderBytes) - i : nNumberOfBytesToRead;
            std::memcpy(lpBuffer, &globals::loaderBytes[i], nNumberOfBytesToRead);
            *lpNumberOfBytesRead = nNumberOfBytesToRead;
            i += nNumberOfBytesToRead;

            return TRUE;
        }
    }

    const auto success = PLH::FnCast(globals::oReadFile, &ReadFile)(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

    return success;
}

BOOL WINAPI hkCloseHandle(HANDLE hObject)
{
    if (hObject != INVALID_HANDLE_VALUE && hObject == globals::loaderHandle)
    {
        globals::loaderHandle = INVALID_HANDLE_VALUE;
        globals::bUseMod = false;
    }

    return PLH::FnCast(globals::oCloseHandle, &CloseHandle)(hObject);
}

HANDLE WINAPI hkCreateFile(
    LPCWSTR               lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile
)
{
    auto fileName = std::wstring(lpFileName);

    HANDLE hFile = PLH::FnCast(globals::oCreateFileW, &CreateFileW)(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

    std::wcout << "[CreateFileW] " << lpFileName << std::endl;

    if ((Utils::EndsWith(fileName, L"\\loader.swf") || Utils::EndsWith(fileName, L"/loader.swf"))) // ouverture de loader.swf
    {
        globals::loaderHandle = hFile;
    }

    return hFile;
}

int WINAPI hkRecv(SOCKET s, char* buf, int len, int flags)
{
    int ret = PLH::FnCast(globals::oRecv, &recv)(s, buf, len, flags);

    return ret;
}

int WINAPI hkSend(
    SOCKET s,
    LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent,
    DWORD dwFlags,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
)
{
    auto ret = PLH::FnCast(globals::oSend, &WSASend)(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);

    return ret;
}

int WINAPI hkConnect(SOCKET s, sockaddr* addr, int namelen)
{
    if (addr)
    {
        char ip[1024]{ 0 };

        sockaddr_in* sin = ((sockaddr_in*)addr);
        uint16_t port = htons(sin->sin_port);

        InetNtopA(AF_INET, &sin->sin_addr.s_addr, ip, sizeof(ip));

        if (!std::string(ip).compare("127.0.0.1") || !std::string(ip).compare("0.0.0.0"))
        {
            std::cout << "Skipped socket binding (loopback address)" << std::endl;
        }
        else
        {
            sockaddr_in adapterName{};
            adapterName.sin_addr.s_addr = inet_addr(globals::adapterIp.data());
            adapterName.sin_family = AF_INET;
            adapterName.sin_port = 0;

            bind(s, (sockaddr*)&adapterName, sizeof(adapterName));

            std::cout << "[+] Socket binded to " << globals::adapterIp.data() << std::endl;
        }

        if (port == 26117) // ankama launcher
        {
            globals::launcherSocket = s;
            sin->sin_port = ntohs(globals::launcherPort);
            port = htons(sin->sin_port);
        }
        else if (port == 443 || port == 5555) // game server
        {
            globals::gameSocket = s;
        }

        std::cout << "[+] connecting to " << ip << ":" << port << std::endl;
    }

    return PLH::FnCast(globals::oConnect, &connect)(s, addr, namelen);
}

extern "C" __declspec(dllexport) void HookChild(const wchar_t* modulePath, const char* adapterIp, uint16_t launcherPort, const char* login, const char* password, bool bSpoofId)
{
    globals::modulePath = modulePath,
    globals::launcherPort = launcherPort;
    globals::adapterIp = adapterIp;
    globals::login = login;
    globals::password = password;
    globals::bSpoofId = bSpoofId;

    globals::detourRecv.emplace((uint64_t)&recv, (uint64_t)hkRecv, &globals::oRecv, globals::dis.value());
    globals::detourRecv->hook();

    globals::detourSend.emplace((uint64_t)&WSASend, (uint64_t)hkSend, &globals::oSend, globals::dis.value());
    globals::detourSend->hook();

    globals::detourConnect.emplace((uint64_t)&connect, (uint64_t)hkConnect, &globals::oConnect, globals::dis.value());
    globals::detourConnect->hook();

    if (bSpoofId)
    {
        globals::detourGetHostName.emplace((uint64_t)&gethostname, (uint64_t)hkGetHostName, &globals::oGetHostName, globals::dis.value());
        globals::detourGetHostName->hook();

        globals::detourCreateProcess.emplace((uint64_t)&CreateProcessW, (uint64_t)hkCreateProcess, &globals::oCreateProcess, globals::dis.value());
        globals::detourCreateProcess->hook();
    }

}

extern "C" __declspec(dllexport) void HookProcess(const wchar_t* modulePath, const char* adapterIp, uint16_t launcherPort, const char* login, const char* password, bool bUseMod, int clientArch, bool bSpoofId)
{
    //    
    //AllocConsole();
    //freopen("CONOUT$", "w", stdout);
    //

    globals::modulePath = modulePath;
    globals::launcherPort = launcherPort;
    globals::adapterIp = adapterIp;
    globals::login = login;
    globals::password = password;
    globals::bUseMod = bUseMod;
    globals::bSpoofId = bSpoofId;

    globals::detourGetFileInfo.emplace((uint64_t)&GetFileAttributesExW, (uint64_t)hkGetFileAttributesExW, &globals::oGetFileInfo, globals::dis.value());
    globals::detourGetFileInfo->hook();

    globals::detourReadFile.emplace((uint64_t)&ReadFile, (uint64_t)hkReadFile, &globals::oReadFile, globals::dis.value());
    globals::detourReadFile->hook();

    globals::detourCreateFile.emplace((uint64_t)&CreateFileW, (uint64_t)hkCreateFile, &globals::oCreateFileW, globals::dis.value());
    globals::detourCreateFile->hook();

    globals::detourCloseHandle.emplace((uint64_t)&CloseHandle, (uint64_t)hkCloseHandle, &globals::oCloseHandle, globals::dis.value());
    globals::detourCloseHandle->hook();

    if (clientArch == 1) // legacy
    {
        HookChild(modulePath, adapterIp, launcherPort, login, password, false);
    }
    else
    {
        globals::detourCreateProcess.emplace((uint64_t)&CreateProcessW, (uint64_t)hkCreateProcess, &globals::oCreateProcess, globals::dis.value());
        globals::detourCreateProcess->hook();
    }

    globals::detourSetWindowText.emplace((uint64_t)&SetWindowTextW, (uint64_t)hkSetWindowText, &globals::oSetWindowText, globals::dis.value());
    globals::detourSetWindowText->hook();

    StartEmulator(globals::launcherPort);
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        globals::dis.emplace(PLH::Mode::x86);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

