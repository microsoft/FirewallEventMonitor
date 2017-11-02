// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "UserInput.h"

// CLSIDFromString
#include "Objbase.h"

using namespace FirewallEventMonitor;

void UserInput::PrintUsage() const
{
    wprintf(
        L"FirewallEventMonitor.exe \n"
        "  -TimeLimit <seconds> : Stop after running for the specified time. Default: %d seconds. \n"
        "  -NoTimeout : Run until forcibly  stopped.\n"
        "  -EventThrottle <count> : Throttle events captured per second. Default: %d. \n"
        "  -Output <output1,output2,...> : Comma-delimited list of desired output.\n"
        "    Console : Print to console.\n"
        "    File : Write to file on disk.\n"
        "  -Directory <path> : Location of log file (if -Output generates one). Default: current directory.\n"
        "  -IP <address1,address2,...> : Fitler for the comma-delimited list of addresses.\n"
        "    Note: Events without the specified IP address(es) in either source or destination are ignored.\n"
        "  -Rule <guid1,guid2,...> : Fitler for the comma-delimited list of Rule Ids.\n"
        "    Note: Events without the specified Rule Ids are ignored. \n"
        "    Note: Must be valid Guids. XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX or \"{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}\" \n"
        "\n",
        Parameters::DefaultTimeLimitInSeconds,
        Parameters::DefaultEventCountMaxPerSecond);
}

ArgumentParsingResults UserInput::ParseArguments(
    const int argc,
    _In_reads_(argc) const wchar_t** argv
)
try
{
    bool success = true;

    // ignore the first argv (the exe itself)
    const wchar_t** arg_begin = argv + 1;
    const wchar_t** arg_end = argv + argc;
    std::vector<const wchar_t*> args(arg_begin, arg_end);

    // if '-help' was parsed, exit
    if (ParseHelp(args))
    {
        return ArgumentParsingResults::Help;
    }

    if (!ParseEventThrottle(args))
    {
        success = false;
    }

    if (!ParseTimeLimit(args))
    {
        success = false;
    }

    if (!ParseNoTimeout(args))
    {
        success = false;
    }

    if (!ParseOutput(args))
    {
        success = false;
    }

    if (!ParseDirectory(args))
    {
        success = false;
    }

    if (!ParseIpAddressFilters(args))
    {
        success = false;
    }

    if (!ParseRuleIdFilters(args))
    {
        success = false;
    }

    if (!success)
    {
        wprintf(L"Parsing arguments failed.\n");
        PrintUsage();
        return ArgumentParsingResults::Fail;
    }

    return  ArgumentParsingResults::Success;
}
catch (const std::exception &ex)
{
    wprintf(L"Attempting to parse arguments raised exception: %S.\n", ex.what());
    return ArgumentParsingResults::Fail;
}

bool UserInput::ParseHelp(
    const std::vector<const wchar_t*>& _args)
{
    // Example: -Help, -?
    bool helpFound = ArgumentProcessing::FindParameter(_args, L"-Help");
    bool questionmarkFound = ArgumentProcessing::FindParameter(_args, L"-?");
    if (helpFound ||
        questionmarkFound)
    {
        PrintUsage();
        return true;
    }
    return false;
}

bool UserInput::ParseEventThrottle(
    const std::vector<const wchar_t*>& _args)
{
    // Example: -MaxEvents 5
    std::wstring numEvents;
    bool foundEvents = ArgumentProcessing::FindParameter(_args, L"-EventThrottle", true, &numEvents);
    if (!foundEvents)
    {
        return true;
    }

    m_Parameters.maxEventsPerEpoc = std::stoul(numEvents);
    wprintf(L"\tEventThrottle: limiting collection to %d events per second.\n", m_Parameters.maxEventsPerEpoc);

    return true;
}

bool UserInput::ParseTimeLimit(
    const std::vector<const wchar_t*>& _args)
{
    // Example: -TimeLimit 60
    std::wstring seconds;
    bool foundTimeLimit = ArgumentProcessing::FindParameter(_args, L"-TimeLimit", true, &seconds);
    if (!foundTimeLimit)
    {
        return true;
    }

    m_Parameters.maxRuntimeInSeconds = std::stoul(seconds);
    wprintf(L"\tTimeLimit: limiting runtime to %d seconds.\n", m_Parameters.maxRuntimeInSeconds);

    return true;
}

bool UserInput::ParseNoTimeout(
    const std::vector<const wchar_t*>& _args)
{
    // Example: -NoTimeout
    bool noTimeoutFound = ArgumentProcessing::FindParameter(_args, L"-NoTimeout");
    if (noTimeoutFound)
    {
        m_Parameters.noTimeout = true;
        wprintf(L"\tNoTimeout: program will run until forcibly stopped.\n");
    }
    return true;
}

bool UserInput::ParseOutput(
    const std::vector<const wchar_t*>& _args)
{
    // Example: -Output Console
    // Example: -Output Console,File
    std::wstring outputs;
    bool foundOutput = ArgumentProcessing::FindParameter(_args, L"-Output", true, &outputs);
    if (!foundOutput)
    {
        return true;
    }

    // Undo console default if Output is specified.
    m_Parameters.outputToConsole = false;

    ValidationFunction func = [&](const std::wstring& input)->bool 
    { return ValidateOutputType(input); };

    bool valid = ValidateCommaDelimitedInput(
        outputs,
        func);

    return valid;
}

bool UserInput::ParseDirectory(
    const std::vector<const wchar_t*>& _args)
{
    // Example: -Directory C:\temp
    std::wstring directory;
    bool foundDirecotry = ArgumentProcessing::FindParameter(_args, L"-Directory", true, &directory);
    if (!foundDirecotry)
    {
        return true;
    }

    // No validation at the moment.
    m_Parameters.logDirectory.assign(directory);
    return true;
}

bool UserInput::ParseIpAddressFilters(
    const std::vector<const wchar_t*>& _args)
{
    // Example: -IP <addr>
    // Example: -IP <addr>,<addr2> ...
    std::wstring addresses;
    bool foundIp = ArgumentProcessing::FindParameter(_args, L"-IP", true, &addresses);
    if (!foundIp)
    {
        return true;
    }

    ValidationFunction func = [&](const std::wstring& input)->bool
    { return ValidateIpAddress(input); };

    bool valid = ValidateCommaDelimitedInput(
        addresses,
        func);

    if (!valid)
    {
        return false;
    }

    wprintf(L"\tIP: filtering by the following IP addresses [");
    for (const auto& ip : m_Parameters.ipAddressFilters)
    {
        wprintf(L"%ls ", ip.c_str());
    }
    wprintf(L"]\n");
    return true;
}

bool UserInput::ParseRuleIdFilters(
    const std::vector<const wchar_t*>& _args)
{
    // Example: -Rule <id>
    // Example: -Rule <id>,<id2> ...
    std::wstring rules;
    bool foundRule = ArgumentProcessing::FindParameter(_args, L"-Rule", true, &rules);
    if (!foundRule)
    {
        return true;
    }

    ValidationFunction func = [&](const std::wstring& input)->bool
    { return ValidateRuleId(input); };

    bool valid = ValidateCommaDelimitedInput(
        rules,
        func);

    if (!valid)
    {
        return false;
    }

    wprintf(L"\tRule: filtering by the following Rule Ids [");
    for (const auto& ruleFilter : m_Parameters.ruleIdFilters)
    {
        wprintf(L"%ls ", ruleFilter.c_str());
    }
    wprintf(L"]\n");
    return true;
}

bool UserInput::ValidateOutputType(
    const std::wstring& value)
{
    if (ntl::String::iordinal_equals(value, L"Console"))
    {
        wprintf(L"\tOutput: printing to Console.\n");
        m_Parameters.outputToConsole = true;
    }
    else if (ntl::String::iordinal_equals(value, L"File"))
    {
        wprintf(L"\tOutput: writing to File.\n");
        m_Parameters.outputToFile = true;
    }
    else
    {
        wprintf(L"Unrecognized output type specified: %ls.\n", value.c_str());
        return false;
    }
    return true;
}

bool UserInput::ValidateIpAddress(
    const std::wstring& ipAddress)
{
    // No validation at the moment.
    m_Parameters.ipAddressFilters.push_back(ipAddress);
    return true;
}

bool UserInput::ValidateRuleId(
    const std::wstring& rule)
{
    // Test for Guid in the form: {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
    GUID guid;
    HRESULT hr = CLSIDFromString(rule.c_str(), &guid);
    if (SUCCEEDED(hr))
    {
        std::wstring trimmed = rule;
        // Trim '{'
        trimmed.erase(trimmed.begin());
        // Trim '}'
        trimmed.pop_back();
        m_Parameters.ruleIdFilters.push_back(trimmed);
        return true;
    }

    // Test for Guid in the form: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    std::wstring ruleWithBraces = L"{" + rule + L"}";
    hr = CLSIDFromString(ruleWithBraces.c_str(), &guid);
    if (SUCCEEDED(hr))
    {
        m_Parameters.ruleIdFilters.push_back(rule);
        return true;
    }

    wprintf(L"Invalid Guid for RuleId: %ls.\n", rule.c_str());
    return false;
}

bool UserInput::ValidateCommaDelimitedInput(
    const std::wstring& input,
    _In_ ValidationFunction matchFunction)
{
    auto commas = ntl::String::all_indices_of(
        input.begin(), input.end(),
        [](wchar_t _ch) -> bool { return _ch == L','; });

    // Only one input was specified
    if (commas.size() == 0)
    {
        return matchFunction(input);
    }

    // Multiple comma-delimited inputs were specified
    bool success = true;
    std::wstring::const_iterator beginningOfWord = std::cbegin(input);
    for (const auto& comma : commas)
    {
        std::wstring inputElement(beginningOfWord, comma);
        auto result = matchFunction(inputElement);

        if (!result)
        {
            success = false;
        }

        // move beginningOfWord to the first character of the next word
        beginningOfWord = comma;
        ++beginningOfWord;
    }

    // beginningOfWord now points to the first character of the very last word
    std::wstring::const_iterator endOfWord = std::end(input);
    std::wstring lastInput(beginningOfWord, endOfWord);
    auto result = matchFunction(lastInput);

    if (!result)
    {
        success = false;
    }

    return success;
}