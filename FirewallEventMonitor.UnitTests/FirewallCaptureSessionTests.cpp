// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include <CppUnitTest.h>
// code under test headers
#include "FirewallCaptureSession.h"
// c++ headers
#include <memory>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FirewallEventMonitor;

namespace FirewallEventMonitorUnitTest
{
    TEST_CLASS(FirewallCaptureSessionTests)
    {
    public:

        TEST_METHOD_INITIALIZE(MethodInit)
        {
            m_Params.outputToConsole = false;
        }

        TEST_METHOD(EmptyFilterListMatchesAnyAddress)
        {
            Logger::WriteMessage(L"EmptyFilterListMatchesAnyAddress");

            m_Params.ipAddressFilters.clear();
            FirewallCaptureSession reader(m_Params);
            bool result = reader.MatchIpAddressFilter(correctAddress);

            Assert::IsTrue(result);
        }

        TEST_METHOD(FilterMatchesCorrectAddress)
        {
            Logger::WriteMessage(L"FilterMatchesCorrectAddress");

            m_Params.ipAddressFilters.clear();
            m_Params.ipAddressFilters.push_back(correctAddress);
            FirewallCaptureSession reader(m_Params);
            bool result = reader.MatchIpAddressFilter(correctAddress);

            Assert::IsTrue(result);
        }

        TEST_METHOD(FilterDropsIncorrectAddress)
        {
            Logger::WriteMessage(L"FilterDropsIncorrectAddress");

            m_Params.ipAddressFilters.clear();
            m_Params.ipAddressFilters.push_back(correctAddress);
            FirewallCaptureSession reader(m_Params);
            bool result = reader.MatchIpAddressFilter(incorrectAddress);

            Assert::IsFalse(result);
        }

        TEST_METHOD(EmptyFilterListMatchesAnyRule)
        {
            Logger::WriteMessage(L"EmptyFilterListMatchesAnyRule");

            m_Params.ruleIdFilters.clear();
            FirewallCaptureSession reader(m_Params);
            bool result = reader.MatchRuleIdFilter(correctRuleId);

            Assert::IsTrue(result);
        }

        TEST_METHOD(FilterMatchesCorrectRule)
        {
            Logger::WriteMessage(L"FilterMatchesCorrectRule");

            m_Params.ruleIdFilters.clear();
            m_Params.ruleIdFilters.push_back(correctRuleId);
            FirewallCaptureSession reader(m_Params);
            bool result = reader.MatchRuleIdFilter(correctRuleId);

            Assert::IsTrue(result);
        }

        TEST_METHOD(FilterDropsIncorrectRule)
        {
            Logger::WriteMessage(L"FilterDropsIncorrectRule");

            m_Params.ruleIdFilters.clear();
            m_Params.ruleIdFilters.push_back(correctRuleId);
            FirewallCaptureSession reader(m_Params);
            bool result = reader.MatchRuleIdFilter(incorrectRuleId);

            Assert::IsFalse(result);
        }

    private:
        Parameters m_Params;

        std::wstring correctAddress = L"100.100.100.100";
        std::wstring incorrectAddress = L"200.200.200.200";

        std::wstring correctRuleId = L"29959cda-8d97-48ea-92ce-4c0164aac7f4";
        std::wstring incorrectRuleId = L"1bd92312-2f5d-447b-b2b3-90edc728b374";
    };
}