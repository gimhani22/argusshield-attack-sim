// Logger.cpp
#include "Logger.h"
#include <fstream>
#include <iostream>
#include <windows.h>

void LogActivity(const std::string& message)
{
    // Write to log file
    std::ofstream logFile(ARGUSSHIELD_LOG_FILE, std::ios::app);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char timestamp[64];
    sprintf_s(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    logFile << timestamp << message << std::endl;
    logFile.close();

    // Also print to console
    std::cout << message << std::endl;
}
