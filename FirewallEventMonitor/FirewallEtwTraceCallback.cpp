// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "FirewallEtwTraceCallback.h"
#include "FirewallCaptureSession.h"

namespace FirewallEventMonitor
{
    const INT IPV4_RULE_MATCH_EVENT_ID = 400;
    const INT IPV6_RULE_MATCH_EVENT_ID = 401;
    const INT IPV4_ICMP_RULE_MATCH_EVENT_ID = 402;

    FirewallEtwTraceCallback::FirewallEtwTraceCallback(
        const std::weak_ptr<FirewallCaptureSession> eventWatcher,
        const Parameters &parameters,
        const std::shared_ptr<FileLogger> fileLogger,
        const std::shared_ptr<Timer> timer,
        const std::shared_ptr<EventCounter> eventCounter)
        : m_EventWatcher(eventWatcher),
        m_Parameters(parameters),
        m_FileLogger(fileLogger),
        m_Timer(timer),
        m_EventCounter(eventCounter)
    {
    }

    bool FirewallEtwTraceCallback::operator()(
        const PEVENT_RECORD pEventRecord) try
    {
        if (m_EventCounter->EpocEventCountLimitReached() ||
            m_Timer->TimeLimitReached())
        {
            return false;
        }

        ntl::EtwRecord record(pEventRecord);

        return ProcessEventRecord(record);
    }
    catch (const std::exception &ex)
    {
        wprintf(L"Exception: %S.\n", ex.what());
        return false;
    }

    bool FirewallEtwTraceCallback::ProcessEventRecord(
        const ntl::EtwRecord& record)
    {
        INT eventId = record.getEventId();
        bool vfpEventIdMatch =
            eventId == IPV4_RULE_MATCH_EVENT_ID ||
            eventId == IPV6_RULE_MATCH_EVENT_ID ||
            eventId == IPV4_ICMP_RULE_MATCH_EVENT_ID;
        if (!vfpEventIdMatch)
        {
            return false;
        }

        auto captureSession = m_EventWatcher.lock();
        if (!captureSession)
        {
            return false;
        }

        VfpEventData eventData = CollectEventData(record);

        // If Ip Filters were specified, filter out events
        //     where neither the Source nor Destination match.
        bool sourceNotMatching =
            !eventData.source.empty() &&
            !captureSession->MatchIpAddressFilter(eventData.source);
        bool destinationNotMatching =
            !eventData.destination.empty() &&
            !captureSession->MatchIpAddressFilter(eventData.destination);
        if (sourceNotMatching && destinationNotMatching)
        {
            return false;
        }

        // If RuleId Filters were specified, filter out events
        //     where the RuleId does not match.
        if (!captureSession->MatchRuleIdFilter(eventData.ruleId))
        {
            return false;
        }

        if (m_Parameters.outputToConsole)
        {
            OutputToConsole(eventData);
        }

        if (m_Parameters.outputToFile)
        {
            OutputToFile(eventData);
        }

        m_EventCounter->IncrementEventCount();

        return true;
    }

    VfpEventData FirewallEtwTraceCallback::CollectEventData(
        const ntl::EtwRecord& record)
    {
        VfpEventData eventData;

        record.queryEventProperty(L"SrcIpv4Addr", eventData.source);
        record.queryEventProperty(L"DstIpv4Addr", eventData.destination);

        if (eventData.source.empty() && eventData.destination.empty())
        {
            record.queryEventProperty(L"SrcIpv6Addr", eventData.source);
            record.queryEventProperty(L"DstIpv6Addr", eventData.destination);
        }

        Timer::GetDateAndTime(record.getTimeStamp(), &eventData.date, &eventData.time);

        {
            std::wstring direction;
            record.queryEventProperty(L"Direction", direction);
            if (direction.empty())
            {
                wprintf(L"Warning: Direction empty.\n");
            }
            else
            {
                // Translate Direction for certain values.
                int i = std::stoi(direction);
                switch (i)
                {
                case 0: eventData.direction = L"Outbound"; break;
                case 1: eventData.direction = L"Inbound"; break;
                default: wprintf(L"Warning: Direction %i did not match expected values.\n", i);
                }
            }
        }

        {
            std::wstring ruleType;
            record.queryEventProperty(L"RuleType", ruleType);
            if (ruleType.empty())
            {
                wprintf(L"Warning: RuleType empty.\n");
            }
            else
            {
                // Translate Rule Type for certain values.
                int i = std::stoi(ruleType);
                switch (i)
                {
                case 1: eventData.ruleType = L"Allow"; break;
                case 2: eventData.ruleType = L"Deny"; break;
                default: wprintf(L"Warning: RuleType %i did not match expected values.\n", i);
                }
            }
        }

        {
            std::wstring protocol;
            record.queryEventProperty(L"IpProtocol", protocol);
            if (protocol.empty())
            {
                wprintf(L"Warning: IpProtocol empty.\n");
            }
            else
            {
                // Translate Protocol for certain values.
                int i = std::stoi(protocol);
                switch (i)
                {
                case 0: eventData.protocol = L"HOPOPT"; break;
                case 1: eventData.protocol = L"ICMPv4"; break;
                case 2: eventData.protocol = L"IGMP"; break;
                case 6: eventData.protocol = L"TCP"; break;
                case 17: eventData.protocol = L"UDP"; break;
                case 41: eventData.protocol = L"IPv6"; break;
                case 43: eventData.protocol = L"IPv6Route"; break;
                case 44: eventData.protocol = L"IPv6Frag"; break;
                case 47: eventData.protocol = L"GRE"; break;
                case 58: eventData.protocol = L"ICMPv6"; break;
                case 59: eventData.protocol = L"IPv6NoNxt"; break;
                case 60: eventData.protocol = L"IPv6Opts"; break;
                case 256: eventData.protocol = L"ANY"; break;
                default: wprintf(L"Warning: IpProtocol %i did not match expected values.\n", i);
                }
            }
        }

        {
            std::wstring icmpType;
            record.queryEventProperty(L"IcmpType", icmpType);
            // IcmpType not always present
            if (!icmpType.empty())
            {
                // Translate ICMP Type for certain values.
                int i = std::stoi(icmpType);
                switch (i)
                {
                case 0: eventData.icmpType = L"V4EchoReply"; break;
                case 5: eventData.icmpType = L"V4Redirect"; break;
                case 8: eventData.icmpType = L"V4EchoRequest"; break;
                case 9: eventData.icmpType = L"V4RouterAdvert"; break;
                case 10: eventData.icmpType = L"V4RouterSolicit"; break;
                case 13: eventData.icmpType = L"V4TimestampRequest"; break;
                case 14: eventData.icmpType = L"V4TimestampReply"; break;
                case 128: eventData.icmpType = L"V6EchoRequest"; break;
                case 129: eventData.icmpType = L"V6EchoReply"; break;
                case 133: eventData.icmpType = L"V6RouterSolicit"; break;
                case 134: eventData.icmpType = L"V6RouterAdvert"; break;
                case 135: eventData.icmpType = L"V6NeighborSolicit"; break;
                case 136: eventData.icmpType = L"V6NeighborAdvert"; break;
                default: wprintf(L"Warning: IcmpType %i did not match expected values.\n", i);
                }
            }
        }

        {
            record.queryEventProperty(L"Status", eventData.status);
            if (eventData.status.compare(L"0x0") == 0)
            {
                eventData.status = L"STATUS_SUCCESS";
            }
        }
        // Port
        record.queryEventProperty(L"PortId", eventData.portId);
        record.queryEventProperty(L"PortName", eventData.portName);
        record.queryEventProperty(L"PortFriendlyName", eventData.portFriendlyName);
        // Flow
        record.queryEventProperty(L"SrcPort", eventData.sourcePort);
        record.queryEventProperty(L"DstPort", eventData.destinationPort);
        record.queryEventProperty(L"IsTcpSyn", eventData.isTcpSyn);
        // Rule
        record.queryEventProperty(L"RuleId", eventData.ruleId);
        record.queryEventProperty(L"LayerId", eventData.layerId);
        record.queryEventProperty(L"GroupId", eventData.groupId);
        record.queryEventProperty(L"GftFlags", eventData.gftFlags);

        return eventData;
    }

    void FirewallEtwTraceCallback::OutputToConsole(
        const VfpEventData& eventData)
    {
        OutputToStream(eventData, stdout);
    }

    void FirewallEtwTraceCallback::OutputToFile(
        const VfpEventData& eventData)
    {
        FILE *logFile = m_FileLogger->GetLogFile();

        if (logFile == NULL)
        {
            wprintf(L"Warning: Unable to log to null file.\n");
            return;
        }

        OutputToStream(eventData, logFile);
    }

    void FirewallEtwTraceCallback::OutputToStream(
        const VfpEventData& eventData,
        _In_ FILE *stream)
    {
        // Header
        fwprintf(stream, L"[%ls %ls] %ls %ls rule status = %ls \n",
            eventData.date.c_str(),
            eventData.time.c_str(),
            eventData.direction.c_str(),
            eventData.ruleType.c_str(),
            eventData.status.c_str());

        // Port
        fwprintf(stream, L"  port {id = %ls, portName = %ls, portFriendlyName = %ls} \n",
            eventData.portId.c_str(),
            eventData.portName.c_str(),
            eventData.portFriendlyName.c_str());

        // Flow
        fwprintf(stream, L"  flow {src = %ls, dst = %ls, protocol = %ls",
            eventData.source.c_str(),
            eventData.destination.c_str(),
            eventData.protocol.c_str());

        if (!eventData.sourcePort.empty())
        {
            fwprintf(stream, L", srcPort = %ls",
                eventData.sourcePort.c_str());
        }

        if (!eventData.destinationPort.empty())
        {
            fwprintf(stream, L", dstPort = %ls",
                eventData.destinationPort.c_str());
        }

        if (!eventData.icmpType.empty())
        {
            fwprintf(stream, L", icmp type = %ls",
                eventData.icmpType.c_str());
        }

        if (!eventData.isTcpSyn.empty())
        {
            fwprintf(stream, L", isTcpSyn = %ls",
                eventData.isTcpSyn.c_str());
        }

        fwprintf(stream, L"} \n");

        // Rule
        fwprintf(stream, L"  rule {id = %ls, layer = %ls, group = %ls, gftFlags = %ls} \n\n",
            eventData.ruleId.c_str(),
            eventData.layerId.c_str(),
            eventData.groupId.c_str(),
            eventData.gftFlags.c_str());
    }
}