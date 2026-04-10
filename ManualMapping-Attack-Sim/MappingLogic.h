// MappingLogic.h
#pragma once
#include <windows.h>

// Performs a full manual mapping of Payload.dll into notepad.exe.
// Returns the target PID if the payload was successfully mapped, 0 otherwise.
DWORD PerformManualMapping();
