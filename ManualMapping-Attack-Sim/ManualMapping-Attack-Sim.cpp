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
    bool success = PerformManualMapping();

    std::cout << "\n";

    if (success)
    {
        std::cout << "[*] Manual Mapping completed successfully.\n";
        std::cout << "[*] Check C:\\ArgusShield_mapping_log.txt for detailed logs.\n";
    }
    else
    {
        std::cout << "[!] Manual Mapping encountered issues.\n";
        std::cout << "[!] Check C:\\ArgusShield_mapping_log.txt for details.\n";
    }

    std::cout << "\n";
    system("pause");
    return 0;
}