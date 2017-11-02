#pragma once
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// c++ headers
#include <vector>
#include <string>
#include <algorithm> //find_if
// ntl headers
#include "ntlString.hpp"

namespace FirewallEventMonitor
{
    class ArgumentProcessing
    {
    public:
        static bool FindParameter(
            const std::vector<const wchar_t*>& _args,
            const std::wstring& param);

        static bool FindParameter(
            const std::vector<const wchar_t*>& _args,
            const std::wstring& param,
            const bool valueExpected,
            _Out_ std::wstring* value);
    };
}