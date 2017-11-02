// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "Timer.h"

namespace FirewallEventMonitor
{
    //
    // QueryPerformanceFrequency for the Timer once, since it will not change.
    //
    namespace InitOnce {

        static INIT_ONCE QpfInitOnce = INIT_ONCE_STATIC_INIT;
        static LARGE_INTEGER QpfTimerFrequency = {};

        static BOOL CALLBACK QpfInitOnceCallback(
            PINIT_ONCE, //initOnce
            PVOID, //parameter
            PVOID*) //context
        {
            QpfTimerFrequency.QuadPart = 0LL;
            (void)QueryPerformanceFrequency(&QpfTimerFrequency);
            return TRUE;
        }

        static LONGLONG GetFrequencyQuadPart()
        {
            return QpfTimerFrequency.QuadPart;
        }
    }

    Timer::Timer(
        unsigned long maxRuntimeInSeconds,
        bool runIndefinitely)
        : m_MaxRuntimeInSeconds(maxRuntimeInSeconds),
        m_NoTimeout(runIndefinitely)
    {
        (void)InitOnceExecuteOnce(&InitOnce::QpfInitOnce, InitOnce::QpfInitOnceCallback, NULL, NULL);
        m_TimerStart = { 0 };
        m_EpocStart = { 0 };

        QueryPerformanceCounter(&m_TimerStart);
    }

    bool Timer::TimeLimitReached() const
    {
        if (m_NoTimeout)
            return false;
        return GetTimeElapsedInSeconds(m_TimerStart) >= m_MaxRuntimeInSeconds;
    }

    double Timer::GetTimeElapsedSinceStartInSeconds() const
    {
        return GetTimeElapsedInSeconds(m_TimerStart);
    }

    double Timer::GetTimeElapsedThisEpocInSeconds() const
    {
        return GetTimeElapsedInSeconds(m_EpocStart);
    }

    void Timer::SetEpocStart()
    {
        QueryPerformanceCounter(&m_EpocStart);
    }

    double Timer::GetTimeElapsedLoggingInSeconds() const
    {
        return GetTimeElapsedInSeconds(m_LogCreated);
    }

    void Timer::SetLogCreated()
    {
        QueryPerformanceCounter(&m_LogCreated);
    }

    double Timer::GetTimeElapsedInSeconds(
        const LARGE_INTEGER& start) const
    {
        LARGE_INTEGER timerStop;
        QueryPerformanceCounter(&timerStop);
        double secs = (double)(timerStop.QuadPart - start.QuadPart) / (double)InitOnce::GetFrequencyQuadPart();
        return secs;
    }

    void Timer::GetDateAndTime(
        const LARGE_INTEGER timeStamp,
        _Out_ std::wstring* date,
        _Out_ std::wstring* time)
    {
        FILETIME fileTime;
        memcpy_s(&fileTime, sizeof(fileTime), &timeStamp, sizeof(timeStamp));
        SYSTEMTIME systemTime;
        FileTimeToSystemTime(&fileTime, &systemTime);
        GetDateAndTime(systemTime, date, time);
    }

    void Timer::GetDateAndTime(
        const SYSTEMTIME systemTime,
        _Out_ std::wstring* date,
        _Out_ std::wstring* time)
    {
        // Date and Time formats are ISO 8601
        WCHAR timeString[128] = { 0 }, dateString[128] = { 0 };
        GetDateFormatEx(LOCALE_NAME_INVARIANT, 0, &systemTime, L"yyyyMMdd", dateString, ARRAYSIZE(dateString) - 1, nullptr);
        GetTimeFormatEx(LOCALE_NAME_INVARIANT, 0, &systemTime, L"HHmmss", timeString, ARRAYSIZE(timeString) - 1);

        date->assign(dateString);
        time->assign(timeString);
    }
}