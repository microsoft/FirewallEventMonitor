// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include <CppUnitTest.h>
// code under test headers
#include "UserInput.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FirewallEventMonitor;

namespace UserInputUnitTest
{
    TEST_CLASS(FirewallEventMonitorTests)
    {
    public:

        TEST_METHOD(ValidateRuleIdRecognizesGuid)
        {
            Logger::WriteMessage(L"ValidateRuleIdRecognizesGuid");

            std::wstring guidOne = L"51b87f66-e400-424a-a649-8a4bdc650eb5";
            bool result = input.ValidateRuleId(guidOne);

            Logger::WriteMessage(guidOne.c_str());
            Assert::IsTrue(result);

            std::wstring guidTwo = L"{51b87f66-e400-424a-a649-8a4bdc650eb5}";
            bool resultTwo = input.ValidateRuleId(guidTwo);

            Logger::WriteMessage(guidTwo.c_str());
            Assert::IsTrue(resultTwo);
        }

        TEST_METHOD(ValidateRuleIdRecognizesInvalidGuid)
        {
            Logger::WriteMessage(L"ValidateRuleIdRecognizesInvalidGuid");

            std::wstring invalidGuid = L"8a4bdc650eb5";
            bool result = input.ValidateRuleId(invalidGuid);
            Assert::IsFalse(result);
        }

        TEST_METHOD(ValidateCommaDelemitedInputValid)
        {
            Logger::WriteMessage(L"ValidateCommaDelemitedInputValid");

            std::wstring output = L"Console,File";
            bool result = input.ValidateCommaDelimitedInput(output,
                [&](const std::wstring& value) { return input.ValidateOutputType(value); });
            Assert::IsTrue(result);
        }

        TEST_METHOD(ParseDirectoryMissingReturnsTrue)
        {
            Logger::WriteMessage(L"ParseDirectoryMissingReturnsTrue");

            args.clear();
            args.push_back(L"-Output");
            args.push_back(L"Console,File");

            bool result = input.ParseDirectory(args);
            Assert::IsTrue(result);
        }

        TEST_METHOD(ParseDirectoryMissingValueEndOfArgs)
        {
            Logger::WriteMessage(L"ParseDirectoryMissingValueEndOfArgs");

            args.clear();
            args.push_back(L"-Directory");

            Assert::ExpectException<std::exception>([&]() { input.ParseDirectory(args); });
        }

        TEST_METHOD(ParseDirectoryMissingValue)
        {
            Logger::WriteMessage(L"ParseDirectoryMissingValue");

            args.clear();
            args.push_back(L"-Directory");
            args.push_back(L"-Output");
            args.push_back(L"File");

            Assert::ExpectException<std::exception>([&]() { input.ParseDirectory(args); });
        }

        TEST_METHOD(ParseArgumentsSucceeds)
        {
            Logger::WriteMessage(L"ParseArgumentsSucceeds");

            args.clear();
            args.push_back(L"FirewallEventMonitor.exe");
            args.push_back(L"-IP");
            args.push_back(L"1.1.1.1,2.2.2.2,3.3.3.3");
            args.push_back(L"-TimeLimit");
            args.push_back(L"60");
            args.push_back(L"-Events");
            args.push_back(L"5");
            args.push_back(L"-Output");
            args.push_back(L"Console,File");
            args.push_back(L"-directory");
            args.push_back(L"C:\\temp");
            args.push_back(L"-Rule");
            args.push_back(L"51b87f66-e400-424a-a649-8a4bdc650eb5,{11112222-3333-4444-5555-666677778888}");

            ArgumentParsingResults result = input.ParseArguments(5,args.data());
            Assert::IsTrue(result == ArgumentParsingResults::Success);
        }

    private:
        UserInput input;
        std::vector<const wchar_t*> args;
    };
}