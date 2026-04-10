// HollowingLogic.h
#pragma once
#include <windows.h>

// Performs the process hollowing attack flow.
// Returns the target PID if successful, 0 otherwise.
DWORD PerformProcessHollowing();
