// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "EventCounter.h"

// ntl headers
#include "ntlException.hpp"
#include "ntlLocks.hpp"

namespace FirewallEventMonitor
{
    EventCounter::EventCounter(unsigned long maxEventsPerEpoc)
        : m_MaxEventsPerEpoc(maxEventsPerEpoc)
    {
        ::InitializeCriticalSectionEx(&m_CriticalSection, 4000, 0);
    }

    unsigned long EventCounter::GetEventCountThisEpoc() const
    {
        return m_EventCountThisEpoc;
    }

    unsigned long EventCounter::GetEventCountTotal() const
    {
        return m_EventCountTotal;
    }

    void EventCounter::IncrementEventCount()
    {
        ntl::AutoReleaseCriticalSection csScoped(&m_CriticalSection);

        m_EventCountThisEpoc++;
        m_EventCountTotal++;

        ntl::FatalCondition(
            (m_EventCountThisEpoc == 0),
            L"m_EventCountThisEpoc overflow");

        // TODO: If FirewallEventMonitor does not have a time limit set, this will get hit at some point.
        ntl::FatalCondition(
            (m_EventCountTotal == 0),
            L"EventCountTotal overflow");
    }

    bool EventCounter::EpocEventCountLimitReached() const
    {
        return m_EventCountThisEpoc >= m_MaxEventsPerEpoc;
    }

    void EventCounter::ResetEpocEventCount()
    {
        ntl::AutoReleaseCriticalSection csScoped(&m_CriticalSection);

        m_EventCountThisEpoc = 0;
    }
}