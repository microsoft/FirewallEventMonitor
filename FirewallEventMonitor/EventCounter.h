// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// OS Headers
#include <Windows.h>

namespace FirewallEventMonitor
{
    class EventCounter
    {
    public:
        EventCounter(unsigned long maxEventsPerEpoc);

        unsigned long GetEventCountThisEpoc() const;

        unsigned long GetEventCountTotal() const;

        void IncrementEventCount();

        bool EpocEventCountLimitReached() const;

        void ResetEpocEventCount();

        EventCounter(EventCounter const&) = delete;
        EventCounter& operator=(EventCounter const&) = delete;
    private:
        CRITICAL_SECTION m_CriticalSection;
        unsigned long m_MaxEventsPerEpoc = 0;
        unsigned long m_EventCountThisEpoc = 0;
        unsigned long m_EventCountTotal = 0;
    };
}