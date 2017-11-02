// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include <CppUnitTest.h>
// code under test headers
#include "Timer.h"
// c++ headers
#include <memory>
#include <fstream>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FirewallEventMonitor;

namespace FirewallEventMonitorUnitTest
{
    TEST_CLASS(FileLoggerTests)
    {
    public:

        TEST_METHOD(TimerDoesNotReachInfiniteLimit)
        {
            Logger::WriteMessage(L"TimerDoesNotReachInfiniteLimit");

            m_Timer = std::make_shared<Timer>(1, true);
            Sleep(1100); // 1.1 second;
            Assert::IsFalse(m_Timer->TimeLimitReached());
        }

        TEST_METHOD(TimerReachesLimit)
        {
            Logger::WriteMessage(L"TimerReachesLimit");

            m_Timer = std::make_shared<Timer>(1);
            Sleep(1100); // 1.1 second;
            Assert::IsTrue(m_Timer->TimeLimitReached());
        }

        TEST_METHOD(DateTimeSetCorrectly)
        {
            Logger::WriteMessage(L"DateTimeSetCorrectly");

            m_Timer = std::make_shared<Timer>(1);
            std::wstring date;
            std::wstring time;
            SYSTEMTIME st;

            st.wYear = 1999;
            st.wMonth = 8;
            st.wDay = 7;
            st.wHour = 11;
            st.wMinute = 2;
            st.wSecond = 33;
            st.wMilliseconds = 0;

            m_Timer->GetDateAndTime(st,&date,&time);
            Logger::WriteMessage(date.c_str());
            Assert::IsTrue(date.compare(L"19990807") == 0);

            Logger::WriteMessage(time.c_str());
            Assert::IsTrue(time.compare(L"110233") == 0);
        }

    private:
        std::shared_ptr<Timer> m_Timer;
    };
}