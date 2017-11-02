// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// os headers
#include <winsock2.h>
// c++ headers
#include <fstream>
// ntl headers
#include "ntlEtwReader.hpp"
#include "ntlEtwRecord.hpp"
#include "ntlEtwRecordQuery.hpp"

#include "Timer.h"
#include "EventCounter.h"
#include "UserInput.h"
#include "FileLogger.h"

namespace FirewallEventMonitor
{
    class FirewallCaptureSession;

    // Collection of Firewall event data.
    struct VfpEventData
    {
    public:
        std::wstring date;
        std::wstring time;
        std::wstring direction;
        std::wstring ruleType;
        std::wstring status;
        // Port
        std::wstring portId;
        std::wstring portName;
        std::wstring portFriendlyName;
        // Flow
        std::wstring source;
        std::wstring destination;
        std::wstring protocol;
        std::wstring sourcePort;
        std::wstring destinationPort;
        std::wstring icmpType;
        std::wstring isTcpSyn;
        // Rule
        std::wstring ruleId;
        std::wstring layerId;
        std::wstring groupId;
        std::wstring gftFlags;
    };

    // Callback function for capturing events.
    struct FirewallEtwTraceCallback
    {
    public:
        FirewallEtwTraceCallback(
            const std::weak_ptr<FirewallCaptureSession> eventWatcher,
            const Parameters &parameters,
            const std::shared_ptr<FileLogger> fileLogger,
            const std::shared_ptr<Timer> timer,
            const std::shared_ptr<EventCounter> eventCounter);

        bool operator()(const PEVENT_RECORD pEventRecord);

        bool ProcessEventRecord(const ntl::EtwRecord& record);

        VfpEventData CollectEventData(const ntl::EtwRecord& record);

        void OutputToConsole(const VfpEventData& eventData);

        void OutputToFile(const VfpEventData& eventData);

    private:
        std::weak_ptr<FirewallCaptureSession> m_EventWatcher;
        Parameters m_Parameters;
        std::shared_ptr<FileLogger> m_FileLogger;
        std::shared_ptr<Timer> m_Timer;
        std::shared_ptr<EventCounter> m_EventCounter;

        void OutputToStream(
            const VfpEventData& eventData,
            _In_ FILE *stream);
    };
}