// pch.h MUST be the first include when using precompiled headers
#include "pch.h"

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <sstream>

// Log file path - consistent with Injector.exe
#define ARGUSSHIELD_LOG_FILE "C:\\ArgusShield_injection_proof.txt"

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
        // Disable thread library calls for optimization
        DisableThreadLibraryCalls(hModule);

        // Gather injection information
        DWORD processId = GetCurrentProcessId();
        DWORD threadId = GetCurrentThreadId();
        std::string processName = GetHostProcessName();
        std::string timestamp = GetTimestamp();

        // Get DLL path
        char dllPath[MAX_PATH];
        GetModuleFileNameA(hModule, dllPath, MAX_PATH);

        // Create detailed log file
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
            logFile << "[*] Timestamp: " << timestamp << std::endl;
            logFile << std::endl;
            logFile << "============================================================" << std::endl;
            logFile.close();
        }

        // Build message for MessageBox
        std::stringstream msg;
        msg << "ArgusShield: LoadLibrary DLL Injection Successful!\n\n";
        msg << "Target Process: " << processName << "\n";
        msg << "Process ID: " << processId << "\n";
        msg << "\nLog saved to: " << ARGUSSHIELD_LOG_FILE;

        // Show message box for visual confirmation
        MessageBoxA(NULL, msg.str().c_str(), "ArgusShield Attack Simulator", MB_OK | MB_ICONWARNING);

        break;
    }
    case DLL_PROCESS_DETACH:
    {
        // Cleanup code when DLL is unloaded (optional logging)
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}