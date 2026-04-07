// Payload.cpp
// ArgusShield Manual Mapping - Payload DLL
// This DLL is manually mapped into notepad.exe without using LoadLibrary.
// When DllMain is called, it shows a MessageBox proving the injection worked.

#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        MessageBoxW(
            NULL,
            L"=== MANUAL MAPPING SUCCESSFUL ===\n\n"
            L"This DLL was NOT loaded by LoadLibrary!\n\n"
            L"It was manually mapped into notepad.exe's memory:\n"
            L"  - PE sections copied manually\n"
            L"  - Imports resolved by hand\n"
            L"  - Relocations applied\n"
            L"  - DllMain called via remote thread\n\n"
            L"Check Task Manager - the DLL does NOT appear\n"
            L"in notepad.exe's module list.\n\n"
            L"Technique: T1055 - Manual Map Injection\n"
            L"Simulator: ArgusShield Attack Simulator",
            L"ArgusShield - Manual Mapping Demo",
            MB_OK | MB_ICONWARNING
        );
        break;

    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
