// Payload.cpp
// ArgusShield Process Hollowing - Payload Executable
// This tiny program is injected into notepad.exe via process hollowing.
// Instead of Notepad opening, this MessageBox appears — proving the process was hollowed.

#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    MessageBoxW(
        NULL,
        L"=== PROCESS HOLLOWING SUCCESSFUL ===\n\n"
        L"This is NOT Notepad!\n\n"
        L"notepad.exe has been hollowed.\n"
        L"This payload is running inside notepad.exe's process space.\n\n"
        L"Check Task Manager - the process name is still notepad.exe,\n"
        L"but it is executing completely different code.\n\n"
        L"Technique: T1055.012 - Process Hollowing\n"
        L"Simulator: ArgusShield Attack Simulator",
        L"ArgusShield - Process Hollowing Demo",
        MB_OK | MB_ICONWARNING
    );

    ExitProcess(0);
    return 0;
}
