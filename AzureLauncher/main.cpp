#define WIN32_LEAN_AND_MEAN
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <d3d9.h>
#include <tchar.h>
#include "Include/IconsFontAwesome5Pro.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#pragma comment (lib, "d3d9")

#include "Include/UI.h"
#include "Include/fonts.h"
#include "Include/UI.h"

#pragma comment(lib, "crypt32")

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    HWND                hwnd;
    MSG                 msg;
    WNDCLASS            wndClass;

    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = 0;
    wndClass.hIcon = 0;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = TEXT("DofusLauncher.UI");

    RegisterClass(&wndClass);

    hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT,
        TEXT("DofusLauncher.UI"),   // window class name
        TEXT("DofusLauncher.UI"),  // window caption
        WS_POPUP,      // window style
        CW_USEDEFAULT,            // initial x position
        CW_USEDEFAULT,            // initial y position
        1,            // initial x size
        1,            // initial y size
        NULL,                     // parent window handle
        NULL,                     // window menu handle
        NULL,                // program instance handle
        NULL);                    // creation parameters

    UpdateWindow(hwnd);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wndClass.lpszClassName, wndClass.hInstance);
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    io.WantSaveIniSettings = false;
    io.IniFilename = nullptr;
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    LauncherUI::Theme();

    ImFontConfig config;
    config.MergeMode = true;
    config.GlyphMinAdvanceX = 16.0f; // Use if you want to make the icon monospaced
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    //io.Fonts->AddFontFromFileTTF("C:/Users/user/AppData/local/Microsoft/Windows/Fonts/roboto-light.ttf", 18);
    io.Fonts->AddFontFromMemoryCompressedTTF(fonts::roboto_regular_compressed_data, fonts::roboto_regular_compressed_size, 16.0f);
    io.Fonts->AddFontFromMemoryCompressedTTF(fonts::fa_brands_400_compressed_data, fonts::fa_brands_400_compressed_size, 24.0f, &config, icon_ranges);
    io.Fonts->AddFontFromMemoryCompressedTTF(fonts::fa_regular_400_compressed_data, fonts::fa_regular_400_compressed_size, 14.0f, &config, icon_ranges);
    io.Fonts->AddFontFromMemoryCompressedTTF(fonts::roboto_black_compressed_data, fonts::roboto_black_compressed_size, 16.0f);
    io.Fonts->AddFontFromMemoryCompressedTTF(fonts::fa_brands_400_compressed_data, fonts::fa_brands_400_compressed_size, 24.0f, &config, icon_ranges);
    io.Fonts->AddFontFromMemoryCompressedTTF(fonts::fa_regular_400_compressed_data, fonts::fa_regular_400_compressed_size, 14.0f, &config, icon_ranges);

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        // style.WindowRounding = 16.0f;
        style.Colors[ImGuiCol_WindowBg].w = 0.0f;
    }

    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(LauncherUI::g_pd3dDevice);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        LauncherUI::Render();

        // Rendering
        ImGui::EndFrame();
        LauncherUI::g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        LauncherUI::g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        LauncherUI::g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
        LauncherUI::g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (LauncherUI::g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            LauncherUI::g_pd3dDevice->EndScene();
        }

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        HRESULT result = LauncherUI::g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

        // Handle loss of D3D9 device
        if (result == D3DERR_DEVICELOST && LauncherUI::g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wndClass.lpszClassName, wndClass.hInstance);

    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    if ((LauncherUI::g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    ZeroMemory(&LauncherUI::g_d3dpp, sizeof(LauncherUI::g_d3dpp));
    LauncherUI::g_d3dpp.Windowed = TRUE;
    LauncherUI::g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    LauncherUI::g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    LauncherUI::g_d3dpp.EnableAutoDepthStencil = TRUE;
    LauncherUI::g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    LauncherUI::g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (LauncherUI::g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &LauncherUI::g_d3dpp, &LauncherUI::g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (LauncherUI::g_pd3dDevice) { LauncherUI::g_pd3dDevice->Release(); LauncherUI::g_pd3dDevice = NULL; }
    if (LauncherUI::g_pD3D) { LauncherUI::g_pD3D->Release(); LauncherUI::g_pD3D = NULL; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = LauncherUI::g_pd3dDevice->Reset(&LauncherUI::g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HDC          hdc;
    PAINTSTRUCT  ps;

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (LauncherUI::g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            LauncherUI::g_d3dpp.BackBufferWidth = LOWORD(lParam);
            LauncherUI::g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}