// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// c++ headers
#include <functional>
#include <vector>
#include <string>

#include "Timer.h"
#include "ArgumentProcessing.h"

namespace FirewallEventMonitor
{
    // Collection of constructor parameters.
    struct Parameters
    {
    public:
        // Event Filtering
        std::vector<std::wstring> ipAddressFilters;
        std::vector<std::wstring> ruleIdFilters;
        // Event Counter
        unsigned long maxEventsPerEpoc = DefaultEventCountMaxPerSecond;
        // Timer
        unsigned long maxRuntimeInSeconds = DefaultTimeLimitInSeconds;
        bool noTimeout = false; // Indefinite runtime.
        // FileLogger
        std::wstring logDirectory = L""; // Defaults to current directory
        bool outputToConsole = true;
        bool outputToFile = false;

        // Constants
        static const unsigned long DefaultTimeLimitInSeconds = 300ul; // 5 Minutes (ignored if noTimeout is true).
        static const unsigned long DefaultEventCountMaxPerSecond = 10000ul; // 10,000 Events.
    };

    enum class ArgumentParsingResults { Success, Fail, Help };

    class UserInput
    {
    public:
        FirewallEventMonitor::Parameters GetParameters() const
        {
            return m_Parameters;
        }

        //
        // User Input Parsing
        //

        void PrintUsage() const;

        ArgumentParsingResults ParseArguments(
            const int argc,
            _In_reads_(argc) const wchar_t** argv);

        bool ParseHelp(const std::vector<const wchar_t*>& _args);

        bool ParseEventThrottle(const std::vector<const wchar_t*>& _args);

        bool ParseTimeLimit(const std::vector<const wchar_t*>& _args);

        bool ParseNoTimeout(const std::vector<const wchar_t*>& _args);

        bool ParseOutput(const std::vector<const wchar_t*>& _args);

        bool ParseDirectory(const std::vector<const wchar_t*>& _args);

        bool ParseIpAddressFilters(const std::vector<const wchar_t*>& _args);

        bool ParseRuleIdFilters(const std::vector<const wchar_t*>& _args);

        //
        // User Input Validation
        //
        typedef std::function<bool(const std::wstring& value)> ValidationFunction;

        // Match text to an OutputFlag, set reader parameters.
        bool ValidateOutputType(const std::wstring& outputType);

        // Add Ip Addresses to reader parameters.
        bool ValidateIpAddress(const std::wstring& ipAddress);

        // Checks RuleId is a valid Guid, adds it to reader parameters.
        bool ValidateRuleId(const std::wstring& ruleId);

        // Validates Comma-Delimited Input using the provided ValidationFunction.
        bool ValidateCommaDelimitedInput(
            const std::wstring& input,
            _In_ ValidationFunction matchFunction);

    private:
        Parameters m_Parameters;
    };
}