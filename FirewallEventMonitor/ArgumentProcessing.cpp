// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "ArgumentProcessing.h"

using namespace FirewallEventMonitor;

bool ArgumentProcessing::FindParameter(
    const std::vector<const wchar_t*>& _args,
    const std::wstring& param)
{
    std::wstring val;
    return FindParameter(_args, param, false, &val);
}


bool ArgumentProcessing::FindParameter(
    const std::vector<const wchar_t*>& _args,
    const std::wstring& param,
    const bool valueExpected,
    _Out_ std::wstring *value)
{
    *value = L"";

    auto iterator = find_if(
        begin(_args),
        end(_args),
        [&param](const wchar_t* _arg) ->
        bool { return (ntl::String::iordinal_equals(_arg, param)); }
    );
    if (iterator == end(_args))
    {
        return false;
    }

    if (!valueExpected)
    {
        return true;
    }

    ++iterator; // skip '-Param'
    if (iterator == end(_args))
    {
        throw std::exception("Value not present. End of arguments reached.");
    }

    *value = *iterator;
    std::size_t foundDash = value->find(L"-");
    if (foundDash != std::string::npos)
    {
        throw std::exception("Value not present. Found another argument instead.");
    }

    return true;
}