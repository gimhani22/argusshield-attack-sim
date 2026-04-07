// ProcessHollowing-Attack-Sim.cpp
// ArgusShield Attack Simulator - Process Hollowing (T1055.012)
// GUI-based fake installer that performs process hollowing when "Install" is clicked.

#include "framework.h"
#include "ProcessHollowing-Attack-Sim.h"
#include "Logger.h"
#include "HollowingLogic.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <thread>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ============================================================
// Window Controls
// ============================================================
HWND hwndMain;
HWND hwndWelcomeText;
HWND hwndDescriptionText;
HWND hwndProgressBar;
HWND hwndStatusText;
HWND hwndProgressDetail;
HWND hwndInstallBtn;
HWND hwndCancelBtn;
HWND hwndFinishBtn;

// State
bool isInstalling = false;
bool hollowingSuccessful = false;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void StartInstallation();

// ============================================================
// Entry Point
// ============================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    const wchar_t CLASS_NAME[] = L"ProcessHollowingSimClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    // Create window - disguised as a system utility installer
    hwndMain = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Windows System Optimizer Pro - Installation Wizard",
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

// ============================================================
// Create GUI Controls
// ============================================================
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

    // Header background (owner-drawn)
    CreateWindow(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, 600, 80,
        hwnd, (HMENU)100, nullptr, nullptr);

    // Logo circle
    HWND hwndLogo = CreateWindow(L"STATIC", L"\x25CF",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 15, 50, 50,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndLogo, WM_SETFONT, (WPARAM)CreateFont(48, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"), TRUE);

    // Title in header
    HWND hwndHeaderTitle = CreateWindow(L"STATIC", L"System Optimizer Pro",
        WS_CHILD | WS_VISIBLE,
        80, 20, 400, 30,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndHeaderTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

    // Subtitle
    HWND hwndSubtitle = CreateWindow(L"STATIC", L"Advanced System Performance Suite",
        WS_CHILD | WS_VISIBLE,
        80, 50, 400, 20,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndSubtitle, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    // Welcome text
    hwndWelcomeText = CreateWindow(L"STATIC", L"Welcome to System Optimizer Pro Setup",
        WS_CHILD | WS_VISIBLE,
        40, 120, 520, 30,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndWelcomeText, WM_SETFONT, (WPARAM)CreateFont(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"), TRUE);

    // Description
    hwndDescriptionText = CreateWindow(L"STATIC",
        L"Ready to install System Optimizer Pro.\r\n\r\nClick 'Install' to start the installation.",
        WS_CHILD | WS_VISIBLE,
        40, 160, 520, 60,
        hwnd, nullptr, nullptr, nullptr);
    SendMessage(hwndDescriptionText, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

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

    // Install button
    hwndInstallBtn = CreateWindow(L"BUTTON", L"Install",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        470, 360, 100, 35,
        hwnd, (HMENU)3, nullptr, nullptr);
    SendMessage(hwndInstallBtn, WM_SETFONT, (WPARAM)hFontButton, TRUE);

    // Cancel button
    hwndCancelBtn = CreateWindow(L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 360, 100, 35,
        hwnd, (HMENU)2, nullptr, nullptr);
    SendMessage(hwndCancelBtn, WM_SETFONT, (WPARAM)hFontButton, TRUE);

    // Finish button (hidden initially)
    hwndFinishBtn = CreateWindow(L"BUTTON", L"Finish",
        WS_CHILD | BS_PUSHBUTTON,
        470, 360, 100, 35,
        hwnd, (HMENU)4, nullptr, nullptr);
    SendMessage(hwndFinishBtn, WM_SETFONT, (WPARAM)hFontButton, TRUE);
}

// ============================================================
// Window Procedure
// ============================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hwndStatic = (HWND)lParam;

        // Header background (dark teal)
        if (GetDlgCtrlID(hwndStatic) == 100)
        {
            SetBkColor(hdcStatic, RGB(0, 77, 64));
            return (LRESULT)CreateSolidBrush(RGB(0, 77, 64));
        }

        // White text for header elements
        if (hwndStatic == FindWindowEx(hwnd, nullptr, L"STATIC", L"System Optimizer Pro") ||
            hwndStatic == FindWindowEx(hwnd, nullptr, L"STATIC", L"Advanced System Performance Suite"))
        {
            SetTextColor(hdcStatic, RGB(255, 255, 255));
            SetBkColor(hdcStatic, RGB(0, 77, 64));
            return (LRESULT)CreateSolidBrush(RGB(0, 77, 64));
        }

        // Logo color (green accent)
        if (hwndStatic == FindWindowEx(hwnd, nullptr, L"STATIC", L"\x25CF"))
        {
            SetTextColor(hdcStatic, RGB(76, 175, 80));
            SetBkColor(hdcStatic, RGB(0, 77, 64));
            return (LRESULT)CreateSolidBrush(RGB(0, 77, 64));
        }

        // Default: transparent background
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        // Install button clicked
        if (wmId == 3 && wmEvent == BN_CLICKED)
        {
            if (!isInstalling)
            {
                StartInstallation();
            }
        }

        // Cancel / Finish buttons
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
        HBRUSH headerBrush = CreateSolidBrush(RGB(0, 77, 64));
        FillRect(hdc, &headerRect, headerBrush);
        DeleteObject(headerBrush);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================
// Installation Flow (runs on background thread)
// ============================================================
void StartInstallation()
{
    isInstalling = true;

    // Hide initial UI
    ShowWindow(hwndWelcomeText, SW_HIDE);
    ShowWindow(hwndDescriptionText, SW_HIDE);
    ShowWindow(hwndInstallBtn, SW_HIDE);

    // Show progress UI
    ShowWindow(hwndStatusText, SW_SHOW);
    ShowWindow(hwndProgressBar, SW_SHOW);
    ShowWindow(hwndProgressDetail, SW_SHOW);

    // Start installation in separate thread
    std::thread installThread([]() {

        const wchar_t* steps[] = {
            L"Extracting files...",
            L"Installing core components...",
            L"Configuring system settings...",
            L"Optimizing system performance...",   // PROCESS HOLLOWING HAPPENS HERE
            L"Registering components...",
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

            if (i == 3)
            {
                LogActivity("Starting Process Hollowing phase...");
                hollowingSuccessful = PerformProcessHollowing();
            }
        }

        // Complete
        SendMessage(hwndProgressBar, PBM_SETPOS, 100, 0);
        Sleep(500);

        // Show success
        SetWindowText(hwndStatusText, L"Installation Complete!");
        SetWindowText(hwndProgressDetail, L"System Optimizer Pro has been successfully installed.");

        ShowWindow(hwndProgressBar, SW_HIDE);
        ShowWindow(hwndCancelBtn, SW_HIDE);
        ShowWindow(hwndFinishBtn, SW_SHOW);

        if (hollowingSuccessful)
        {
            MessageBox(hwndMain,
                L"Installation completed successfully!\n\n"
                L"[ArgusShield] Process Hollowing was performed successfully.\n"
                L"Check C:\\ArgusShield_hollowing_log.txt for details.",
                L"System Optimizer Pro - Setup Complete",
                MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            MessageBox(hwndMain,
                L"Installation completed.\n\n"
                L"[ArgusShield] Note: Process Hollowing simulation encountered issues.\n"
                L"Check C:\\ArgusShield_hollowing_log.txt for details.",
                L"System Optimizer Pro - Setup Complete",
                MB_OK | MB_ICONWARNING);
        }

        });

    installThread.detach();
}
