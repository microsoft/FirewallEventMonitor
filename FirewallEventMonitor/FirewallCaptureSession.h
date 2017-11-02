// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// os headers
#include <winsock2.h>
// c++ headers
#include <memory>
#include <utility>
// ntl headers
#include "ntlEtwReader.hpp"
#include "ntlEtwRecord.hpp"
#include "ntlEtwRecordQuery.hpp"

#include "FileLogger.h"
#include "Timer.h"
#include "EventCounter.h"
#include "FirewallEtwTraceCallback.h"

namespace FirewallEventMonitor
{
    class FirewallCaptureSession : public std::enable_shared_from_this<FirewallCaptureSession>
    {
    public:
        FirewallCaptureSession();

        FirewallCaptureSession(const Parameters &params);
        
        FirewallCaptureSession(
            const Parameters &params,
            std::shared_ptr<FileLogger> fileLogger,
            std::shared_ptr<Timer> timer,
            std::shared_ptr<EventCounter> eventCounter);

        ~FirewallCaptureSession();

        void OpenSession();

        void CloseSession();

        bool CaptureSessionRunning() const;

        bool TimeLimitReached() const;

        void LogFileIntervalCheck();

        double GetTimeRemainingInEpoc() const;

        bool EventCountLimitPerEpocReached() const;

        void ResetEpoc();

        // Returns true if the address matches one of the filters, or if there are no filters.
        bool MatchIpAddressFilter(const std::wstring& address) const;

        // Returns true if the rule matches one of the filters, or if there are no filters.
        bool MatchRuleIdFilter(const std::wstring& ruleId) const;

        // Constants
        const double EpocTimeInMilliseconds = 1000.0; // 1 second.

        FirewallCaptureSession(FirewallCaptureSession const&) = delete;
        FirewallCaptureSession& operator=(FirewallCaptureSession const&) = delete;

    private:
        void GenerateTraceSessionName();

        // Helpers
        std::shared_ptr<FileLogger> m_FileLogger;
        std::shared_ptr<Timer> m_Timer;
        std::shared_ptr<EventCounter> m_EventCounter;
        Parameters m_Parameters;
        // Members
        std::unique_ptr<ntl::EtwReader<FirewallEtwTraceCallback>> m_EtwReader;
        std::vector<GUID> m_ProviderGuids;
        std::wstring m_TraceSessionName;
        GUID m_TraceSessionGuid;
        bool m_CaptureSessionRunning;
    };
}