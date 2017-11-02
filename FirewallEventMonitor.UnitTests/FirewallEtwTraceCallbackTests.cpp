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
    TEST_CLASS(FirewallEtwTraceCallbackTests)
    {
    public:

        TEST_METHOD_INITIALIZE(MethodInit)
        {
            Logger::WriteMessage(L"MethodInit");

            m_Params.outputToFile = true;
            m_Params.outputToConsole = false;

            m_EventCounter = std::make_shared<EventCounter>(10000);
            m_Timer = std::make_shared<Timer>(-1);
            m_FileLogger = std::make_shared<FileLogger>(L"");
            m_Reader = std::make_shared<FirewallCaptureSession>(m_Params);
            m_Callback = std::make_shared<FirewallEtwTraceCallback>(
                std::weak_ptr<FirewallCaptureSession>(m_Reader),
                m_Params,
                m_FileLogger,
                m_Timer,
                m_EventCounter);

            // Read EtwRecord from test file.
            std::wstring file = L"..\\..\\..\\TestTraceSession.etl";
            queryEvents.OpenSavedSession(file.c_str());
            queryEvents.WaitForSession();

            // Query for desired allow/block events.
            ntl::EtwRecordQuery query;
            query.matchProviderId(VFP_PROVIDER_GUID);
            query.matchEventId(400);
            query.matchEventId(401);
            query.matchEventId(402);

            if (!queryEvents.FindFirstEvent(query, m_testRecord))
            {
                Logger::WriteMessage(L"Unable to find any event in test ETL file.");
                Assert::IsTrue(false);
            }
        }

        TEST_METHOD(LogFileContainsDate)
        {
            Logger::WriteMessage(L"LogFileContainsDate");

            std::wstring wdate = L"977db5ea6528";
            std::string date = std::string(wdate.begin(), wdate.end());
            
            VfpEventData eventData;
            eventData.date = wdate;

            std::ofstream out;
            out.open("test.txt");

            m_FileLogger->CreateLogFile();
            m_Callback->OutputToFile(eventData);
            m_FileLogger->CloseLogFile();
            
            // Search file for code
            bool outputContainsVal = false;
            std::wstring filePath = m_FileLogger->GetLogFilePath();
            std::ifstream fileInput;
            fileInput.open(filePath.c_str(), std::ios::in);
            while (fileInput)
            {
                std::string line;
                fileInput >> line;

                std::size_t found = line.find(date);

                if (found != std::string::npos)
                {
                    std::string write = "Found: " + line;
                    Logger::WriteMessage(write.c_str());

                    outputContainsVal = true;
                }
            }

            Assert::IsTrue(outputContainsVal);
        }

        TEST_METHOD(CollectEventDataReturnsDate)
        {
            Logger::WriteMessage(L"CollectEventDataReturnsDate");

            VfpEventData eventData = m_Callback->CollectEventData(m_testRecord);

            // Does date match expected?
            std::wstring expectedDate = L"20170914";
            std::size_t found = eventData.date.find(expectedDate);
            if (found == std::string::npos)
            {
                Assert::IsTrue(false);
            }
        }

        TEST_METHOD(ProcessEventRecordReturnsTrue)
        {
            Logger::WriteMessage(L"ProcessEventRecordReturnsTrue");

            bool result = m_Callback->ProcessEventRecord(m_testRecord);
            Assert::IsTrue(result);
        }

        TEST_METHOD(ProcessEventRecordFiltersEventId)
        {
            Logger::WriteMessage(L"ProcessEventRecordFiltersEventId");

            // Query for wrong event.
            ntl::EtwRecordQuery query;
            query.matchProviderId(VFP_PROVIDER_GUID);
            query.matchEventId(110);

            ntl::EtwRecord wrongRecord;
            if (!queryEvents.FindFirstEvent(query, wrongRecord))
            {
                Logger::WriteMessage(L"Unable to find any event in test ETL file.");
                Assert::IsTrue(false);
            }

            bool result = m_Callback->ProcessEventRecord(wrongRecord);
            Assert::IsFalse(result);
        }

    private:
        Parameters m_Params;
        std::shared_ptr<Timer> m_Timer;
        std::shared_ptr<EventCounter> m_EventCounter;
        std::shared_ptr<FileLogger> m_FileLogger;
        std::shared_ptr<FirewallCaptureSession> m_Reader;
        std::shared_ptr<FirewallEtwTraceCallback> m_Callback;
        
        ntl::EtwReader<> queryEvents;
        GUID VFP_PROVIDER_GUID = {
            0x9F2660EA,
            0xCFE7,
            0x428F,
            { 0x98, 0x50, 0xAE, 0xCA, 0x61, 0x26, 0x19, 0xB0 } };
        ntl::EtwRecord m_testRecord;
    };
}