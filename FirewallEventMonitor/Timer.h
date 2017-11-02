// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// os headers
#include <windows.h>

// c++ headers
#include <memory>
#include <string>

namespace FirewallEventMonitor
{
    class Timer
    {
    public:
        Timer(
            unsigned long maxRuntimeInSeconds,
            bool noTimeout = false);

        bool TimeLimitReached() const;

        double GetTimeElapsedSinceStartInSeconds() const;

        double GetTimeElapsedInSeconds(const LARGE_INTEGER& start) const;

        double GetTimeElapsedThisEpocInSeconds() const;

        void SetEpocStart();

        double GetTimeElapsedLoggingInSeconds() const;

        void SetLogCreated();

        static void GetDateAndTime(
            const LARGE_INTEGER timeStamp,
            _Out_ std::wstring* date,
            _Out_ std::wstring* time);

        static void GetDateAndTime(
            const SYSTEMTIME systemTime,
            _Out_ std::wstring* date,
            _Out_ std::wstring* time);

    private:
        // High-Resolution performance counter reads
        LARGE_INTEGER m_TimerStart;
        LARGE_INTEGER m_EpocStart;
        LARGE_INTEGER m_LogCreated;
        const unsigned long m_MaxRuntimeInSeconds;
        const bool m_NoTimeout;
    };
}