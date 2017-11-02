// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

// c++ headers
#include <atomic>
#include <memory>

#include "FirewallCaptureSession.h"

using namespace FirewallEventMonitor;

// Variable to track forced termination.
std::atomic<bool> programTerminated = false;

// CTRL+C handler to signal termination.
BOOL WINAPI CtrlCHandler(DWORD fdwCtrlType)
{
    UNREFERENCED_PARAMETER(fdwCtrlType);
    programTerminated = true;
    return TRUE;
}

INT __cdecl wmain(
    INT argc,
    __in_ecount(argc) const wchar_t** argv
) try
{
    UserInput input;
    ArgumentParsingResults result =
        input.ParseArguments(argc, argv);

    if (result == ArgumentParsingResults::Fail) {
        return ERROR_INVALID_DATA;
    }
    if (result == ArgumentParsingResults::Help) {
        return ERROR_SUCCESS;
    }

    auto parameters = input.GetParameters();
    auto captureSession = std::make_shared<FirewallCaptureSession>(parameters);
    captureSession->OpenSession();

    // CTRL+C handler to signal termination.
    SetConsoleCtrlHandler(CtrlCHandler, TRUE);

    wprintf(L"Events will appear below. Press Ctrl + C to end the session...\n");

    while (captureSession->CaptureSessionRunning())
    {
        if (programTerminated ||
            captureSession->TimeLimitReached())
        {
            break;
        }

        // If logging to file, close log file an open a new one on an interval (1 hour).
        captureSession->LogFileIntervalCheck();

        // Throttle the number of events recorded to prevent performance degredation during DDOS.
        if (captureSession->EventCountLimitPerEpocReached())
        {
            double remainingTime = captureSession->GetTimeRemainingInEpoc();
            if (remainingTime > 0.0)
            {
                wprintf(L"Event limit per epoc reached (%d). Sleeping for %f Milliseconds.\n",
                    parameters.maxEventsPerEpoc,
                    remainingTime);
                DWORD sleep = static_cast<DWORD>(remainingTime);
                Sleep(sleep);
            }
        }
        captureSession->ResetEpoc();
    }

    return ERROR_SUCCESS;
}
catch (const std::exception &ex)
{
    wprintf(L"Exception: %S.\n", ex.what());
    return ERROR_INVALID_DATA;
}