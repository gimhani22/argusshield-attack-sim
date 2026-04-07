// Logger.cpp
#include "framework.h"
#include "Logger.h"
#include <fstream>
#include <windows.h>

void LogActivity(const std::string& message)
{
    std::ofstream logFile(ARGUSSHIELD_LOG_FILE, std::ios::app);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char timestamp[64];
    sprintf_s(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    logFile << timestamp << message << std::endl;
    logFile.close();
}
