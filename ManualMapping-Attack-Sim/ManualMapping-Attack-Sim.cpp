// ManualMapping-Attack-Sim.cpp
// ArgusShield Attack Simulator - Manual Map DLL Injection (T1055)
// Console application that manually maps a DLL into notepad.exe.

#include <iostream>
#include <string>
#include <windows.h>
#include <shellapi.h>
#include "Logger.h"
#include "MappingLogic.h"

// ============================================================
// Admin privilege check and elevation
// ============================================================
bool IsRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
        &ntAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}

bool RelaunchAsAdmin()
{
    wchar_t exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0)
    {
        return false;
    }

    HINSTANCE result = ShellExecuteW(
        NULL,
        L"runas",
        exePath,
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    return reinterpret_cast<INT_PTR>(result) > 32;
}

// ============================================================
// Entry Point
// ============================================================
int main()
{
    // Check for admin privileges
    if (!IsRunningAsAdmin())
    {
        std::cout << "[!] Admin privileges required. Relaunching...\n";
        if (!RelaunchAsAdmin())
        {
            std::cout << "[-] Failed to relaunch with admin privileges.\n";
            return 1;
        }
        return 0;
    }

    std::cout << "================================================\n";
    std::cout << "  ArgusShield - Manual Mapping Attack Simulator  \n";
    std::cout << "  Technique: T1055 - Manual Map DLL Injection    \n";
    std::cout << "================================================\n\n";

    // Perform the manual mapping attack
    DWORD mappedTargetPID = PerformManualMapping();

    std::cout << "\n";

    // Actively poll for ArgusShield blocking for up to 10 seconds
    // The memory scanner runs every 2 seconds, so we need to wait long enough
    std::cout << "[*] Waiting for ArgusShield detection...\n";

    bool wasBlocked = false;

    if (mappedTargetPID > 0)
    {
        for (int poll = 0; poll < 20; poll++)  // 20 x 500ms = 10 seconds
        {
            Sleep(500);

            HANDLE hTarget = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, mappedTargetPID);
            if (!hTarget)
            {
                // Process handle can't be opened — it was terminated
                wasBlocked = true;
                break;
            }

            DWORD exitCode;
            if (!GetExitCodeProcess(hTarget, &exitCode) || exitCode != STILL_ACTIVE)
            {
                CloseHandle(hTarget);
                wasBlocked = true;
                break;
            }
            CloseHandle(hTarget);
        }
    }

    if (mappedTargetPID > 0 && !wasBlocked)
    {
        std::cout << "[*] Manual Mapping completed successfully.\n";
        std::cout << "[*] Check C:\\ArgusShield_mapping_log.txt for detailed logs.\n";
    }
    else
    {
        // Change console text to red for dramatic effect
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        
        std::cout << "\n";
        std::cout << "============================================================\n";
        std::cout << "  [!] ArgusShield has DETECTED and BLOCKED this attack!\n";
        std::cout << "============================================================\n";
        std::cout << "[!] The target process was terminated by ArgusShield.\n";
        std::cout << "[!] Check C:\\ArgusShield_mapping_log.txt for details.\n";
        std::cout << "\n";
        
        // Show actual error dialog
        MessageBoxA(NULL, "ArgusShield has successfully DETECTED and BLOCKED this attack.\n\nThe target process was forcefully terminated.", "ERROR: Attack Blocked", MB_OK | MB_ICONERROR);

        // Show explicit app crash notification before closing
        MessageBoxA(NULL, "The simulator application has encountered a critical access violation and must close.", "Application Crash", MB_OK | MB_ICONHAND);

        ExitProcess(1);
    }

    std::cout << "\n";
    system("pause");
    return 0;
}