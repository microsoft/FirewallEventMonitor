// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "FileLogger.h"
#include "Timer.h"
// ntl headers
#include "ntlString.hpp"

namespace FirewallEventMonitor
{
    const LPCWSTR LOG_FILE_PREFIX =
        L"FirewallEventMonitor";

    FileLogger::FileLogger(const std::wstring &directory)
        : m_LogDirectory(directory)
    {
    }

    FileLogger::~FileLogger()
    {
        CloseLogFile();
    }

    void FileLogger::CreateLogFile()
    {
        if (m_LogFile != NULL)
        {
            throw std::exception("Log file is in use. Cannot create a new file without closing existing file.");
        }

        GenerateLogFilePath();
        auto filePath = GetLogFilePath();

        errno_t result = _wfopen_s(&m_LogFile, filePath.c_str(), L"w");

        if (result != 0 ||
            m_LogFile == NULL)
        {
            std::string errorMessage = "Unable to open log file ";
            errorMessage += ntl::String::convert_to_string(filePath);
            throw std::exception(errorMessage.c_str());
        }

        wprintf(L"\tWriting events to log file: %ls\n", filePath.c_str());
    }

    void FileLogger::CloseLogFile()
    {
        if (m_LogFile == NULL)
        {
            return;
        }

        fclose(m_LogFile);
        m_LogFile = NULL;

        wprintf(L"\tClosed log file: %ls\n", GetLogFilePath().c_str());
    }

    const std::wstring& FileLogger::GetLogDirectory()
    {
        if (m_LogDirectory.empty())
        {
            const DWORD maxLen = MAX_PATH;
            WCHAR path[MAX_PATH] = L"";

            DWORD len = GetCurrentDirectoryW(maxLen, path);

            if (len == 0)
            {
                throw std::exception("Unable to get current directory.");
            }

            m_LogDirectory.assign(path);
        }

        return m_LogDirectory;
    }

    void FileLogger::GenerateLogFilePath()
    {
        // Get directory
        std::wstring filePath(GetLogDirectory());
        filePath.push_back(L'\\');

        // Add name
        filePath.append(LOG_FILE_PREFIX);

        // Add timestamp
        std::wstring date, time;
        SYSTEMTIME systemTime;
        GetSystemTime(&systemTime);
        Timer::GetDateAndTime(systemTime, &date, &time);
        filePath.push_back(L'.');
        filePath.append(date);
        filePath.push_back(L'T'); // ISO 8601
        filePath.append(time);

        // Add file extension
        filePath.append(L".log");

        m_LogFilePath = filePath;
    }

    const std::wstring& FileLogger::GetLogFilePath() const
    {
        return m_LogFilePath;
    }

    FILE* FileLogger::GetLogFile() const
    {
        return m_LogFile;
    }
}