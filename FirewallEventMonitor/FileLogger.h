// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// os headers
#include <winsock2.h>
// c++ headers
#include <utility>
#include <string>

namespace FirewallEventMonitor
{
    class FileLogger
    {
    public:
        FileLogger(const std::wstring &directory);

        ~FileLogger();

        void CreateLogFile();

        void CloseLogFile();

        FILE* GetLogFile() const;

        // Returns user-supplied directory or (if blank) the current directory.
        const std::wstring& GetLogDirectory();

        const std::wstring& GetLogFilePath() const;

        // Constant
        static const DWORD LogFileLimitInSeconds = 3600; // 1 hour.

        FileLogger(FileLogger const&) = delete;
        FileLogger& operator=(FileLogger const&) = delete;
    private:
        FILE *m_LogFile = NULL;
        std::wstring m_LogDirectory;
        std::wstring m_LogFilePath;

        // Appends directory with time-stamped file name.
        void GenerateLogFilePath();
    };
}