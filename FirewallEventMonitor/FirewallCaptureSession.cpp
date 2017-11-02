// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "FirewallCaptureSession.h"

namespace FirewallEventMonitor
{
    const GUID VFP_PROVIDER_GUID = {
        0x9F2660EA,
        0xCFE7,
        0x428F,
        { 0x98, 0x50, 0xAE, 0xCA, 0x61, 0x26, 0x19, 0xB0 } };

    const LPCWSTR TRACE_SESSION_NAME_PREFIX =
        L"FirewallEventCaptureSession";

    FirewallCaptureSession::FirewallCaptureSession()
        : FirewallCaptureSession(Parameters{})
    {
    }

    FirewallCaptureSession::FirewallCaptureSession(const Parameters &params)
        : FirewallCaptureSession(
            params,
            std::make_shared<FileLogger>(params.logDirectory),
            std::make_shared<Timer>(params.maxRuntimeInSeconds, params.noTimeout),
            std::make_shared<EventCounter>(params.maxEventsPerEpoc))
    {
    }

    FirewallCaptureSession::FirewallCaptureSession(
        const Parameters &params,
        std::shared_ptr<FileLogger> fileLogger,
        std::shared_ptr<Timer> timer,
        std::shared_ptr<EventCounter> eventCounter)
        : m_CaptureSessionRunning(false),
        m_FileLogger(fileLogger),
        m_Parameters(params),
        m_Timer(timer),
        m_EventCounter(eventCounter)
    {
        m_ProviderGuids.push_back(VFP_PROVIDER_GUID);

        GenerateTraceSessionName();
    }

    FirewallCaptureSession::~FirewallCaptureSession()
    {
        CloseSession();
    }

    void FirewallCaptureSession::GenerateTraceSessionName()
    {
        m_TraceSessionName = TRACE_SESSION_NAME_PREFIX;

        // Randomly generate a UUID for the TraceSession name and guid.
        // This prevents collisions, allowing multiple instances of FirewallEventMontior to run simultaneously.
        UUID uuid;
        RPC_STATUS uuidcreate_status = UuidCreate(&uuid);
        if (uuidcreate_status == RPC_S_OK)
        {
            m_TraceSessionGuid = uuid;

            // convert UUID to wstring
            WCHAR* wszUuid = NULL;
            RPC_STATUS uuidtostring_status = UuidToStringW(&uuid, (RPC_WSTR*)&wszUuid);
            // Make sure to free wszUuid
            std::unique_ptr<WCHAR, ntl::WCharDeleter> free_string(wszUuid);

            if (uuidtostring_status == RPC_S_OK &&
                wszUuid != NULL)
            {
                m_TraceSessionName += L".";
                m_TraceSessionName += wszUuid;
            }
            else
            {
                throw std::exception("Unable to convert UUID to wstring.");
            }
        }
        else
        {
            throw std::exception("Unable to create UUID.");
        }
    }

    void FirewallCaptureSession::OpenSession()
    {
        m_EtwReader = std::make_unique<ntl::EtwReader<FirewallEtwTraceCallback>>(
            FirewallEtwTraceCallback(
                shared_from_this(),
                m_Parameters,
                m_FileLogger,
                m_Timer,
                m_EventCounter));
        // NULL szFileName to not create a file.
        m_EtwReader->StartSession(m_TraceSessionName.c_str(), NULL, m_TraceSessionGuid);
        m_EtwReader->EnableProviders(m_ProviderGuids);
        m_CaptureSessionRunning = true;
        // Timer
        m_Timer->SetEpocStart();
        // Log
        if (m_Parameters.outputToFile)
        {
            m_FileLogger->CreateLogFile();
            m_Timer->SetLogCreated();
        }
    }

    void FirewallCaptureSession::CloseSession() try
    {
        if (!m_CaptureSessionRunning)
        {
            return;
        }

        // Log
        if (m_Parameters.outputToFile)
        {
            m_FileLogger->CloseLogFile();
        }

        if (!m_EtwReader)
        {
            wprintf(L"Error: Event reader not defined.\n");
            return;
        }

        m_EtwReader->DisableProviders(m_ProviderGuids);
        m_EtwReader->StopSession();
        m_CaptureSessionRunning = false;

        wprintf(L"FirewallEventWatcher ran for %.2f seconds. Captured %d events.\n",
            m_Timer->GetTimeElapsedSinceStartInSeconds(),
            m_EventCounter->GetEventCountTotal());
    }
    catch (const std::exception &ex)
    {
        wprintf(L"Error: DisableProviders raised exception: %S.\n", ex.what());
    }

    bool FirewallCaptureSession::CaptureSessionRunning() const
    {
        return m_CaptureSessionRunning;
    }

    bool FirewallCaptureSession::TimeLimitReached() const
    {
        return m_Timer->TimeLimitReached();
    }

    double FirewallCaptureSession::GetTimeRemainingInEpoc() const
    {
        // Sleep for remainder of the epoc (1 second)
        double timeSpentInSeconds = m_Timer->GetTimeElapsedThisEpocInSeconds();
        double timeSpentInMilliseconds = (timeSpentInSeconds * 1000);
        double remainingTime = FirewallCaptureSession::EpocTimeInMilliseconds - timeSpentInMilliseconds;
        return remainingTime;
    }

    bool FirewallCaptureSession::EventCountLimitPerEpocReached() const
    {
        return m_EventCounter->EpocEventCountLimitReached();
    }

    void FirewallCaptureSession::ResetEpoc()
    {
        m_EventCounter->ResetEpocEventCount();
        m_Timer->SetEpocStart();
    }

    void FirewallCaptureSession::LogFileIntervalCheck()
    {
        if (m_Parameters.outputToFile)
        {
            // If log file is sufficiently old, close it and open a new file.
            double logFileLifetime = m_Timer->GetTimeElapsedLoggingInSeconds();
            if (logFileLifetime >= FileLogger::LogFileLimitInSeconds)
            {
                wprintf(L"LogFile as been open for %.2f seconds. Closing old file and opening a new one.\n",
                    logFileLifetime);
                m_FileLogger->CloseLogFile();
                m_FileLogger->CreateLogFile();
                m_Timer->SetLogCreated();
            }
        }
    }

    bool FirewallCaptureSession::MatchIpAddressFilter(
        const std::wstring& address) const
    {
        if (m_Parameters.ipAddressFilters.empty())
        {
            return true;
        }

        auto found = std::find(
            m_Parameters.ipAddressFilters.begin(),
            m_Parameters.ipAddressFilters.end(),
            address);

        return found != m_Parameters.ipAddressFilters.end();
    }

    bool FirewallCaptureSession::MatchRuleIdFilter(
        const std::wstring& ruleId) const
    {
        if (m_Parameters.ruleIdFilters.empty())
        {
            return true;
        }

        auto found = std::find(
            m_Parameters.ruleIdFilters.begin(),
            m_Parameters.ruleIdFilters.end(),
            ruleId);

        return found != m_Parameters.ruleIdFilters.end();
    }
}