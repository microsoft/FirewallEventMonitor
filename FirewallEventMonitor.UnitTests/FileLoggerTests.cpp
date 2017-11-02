// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include <CppUnitTest.h>
// code under test headers
#include "FirewallCaptureSession.h"
// c++ headers
#include <memory>
#include <fstream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FirewallEventMonitor;

namespace FirewallEventMonitorUnitTest
{
    TEST_CLASS(FileLoggerTests)
    {
    public:

        TEST_METHOD_INITIALIZE(MethodInit)
        {
            m_FileLogger = std::make_shared<FileLogger>(L"");
        }

        TEST_METHOD(LogDirectoryContainsTestDirectory)
        {
            Logger::WriteMessage(L"LogDirectoryContainsTestDirectory");

            std::wstring logFilePath = m_FileLogger->GetLogDirectory();
            Logger::WriteMessage(logFilePath.c_str());

            // Does dir match expected?
            std::wstring expectedDirectory = L"FirewallEventMonitor.UnitTests";
            std::size_t found = logFilePath.find(expectedDirectory);
            if (found == std::string::npos)
            {
                Logger::WriteMessage(L"Expected directory missing.");
                Assert::IsTrue(false);
            }
        }

        TEST_METHOD(LogFileCreateSuccess)
        {
            Logger::WriteMessage(L"LogFileOpenCloseSuccess");

            m_FileLogger->CreateLogFile();
            m_FileLogger->CloseLogFile();

            bool foundLogFile = false;
            std::ifstream my_file(m_FileLogger->GetLogFilePath());
            if (my_file.good())
            {
                foundLogFile = true;
            }
            Assert::IsTrue(foundLogFile);
        }

    private:
        std::shared_ptr<FileLogger> m_FileLogger;
    };
}