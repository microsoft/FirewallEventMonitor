// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <utility>
#include <functional>
// os headers
#include <excpt.h>
#include <Windows.h>
#include <winsock2.h>
// ntl headers
#include "ntlVersionConversion.hpp"
#include "ntlException.hpp"


namespace ntl {

    //
    // not using an unnamed namespace as debugging this is unnecessarily difficult with Windows debuggers
    //
    //
    // typedef used for the std::function to be given to ThreadIocpCallbackInfo
    // - constructed by ThreadIocp
    //
    typedef std::function<void(OVERLAPPED*)> ThreadIocpCallback_t;
    //
    // structure passed to the ThreadIocp IO completion function
    // - to allow the callback function to find the callback
    //   associated with that completed OVERLAPPED* 
    //
    struct ThreadIocpCallbackInfo {
        OVERLAPPED ov;
        PVOID _padding; // required padding before the std::function for the below C_ASSERT alignment/sizing to be correct
        ThreadIocpCallback_t callback;

        // ReSharper disable once CppPossiblyUninitializedMember
        explicit ThreadIocpCallbackInfo(ThreadIocpCallback_t&& _callback)
        : callback(std::move(_callback))
        {
            ::ZeroMemory(&ov, sizeof ov);
        }

        // non-copyable
        ThreadIocpCallbackInfo(const ThreadIocpCallbackInfo&) = delete;
        ThreadIocpCallbackInfo& operator=(const ThreadIocpCallbackInfo&) = delete;
    };
    // asserting at compile time, as we assume this when we reinterpret_cast in the callback
    C_ASSERT(sizeof(ThreadIocpCallbackInfo) == sizeof(OVERLAPPED) +sizeof(PVOID) +sizeof(ThreadIocpCallback_t));


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// ThreadIocp
    ///
    /// class that encapsulates the new-to-Vista ThreadPool APIs around OVERLAPPED IO completion ports
    ///
    /// it creates a handle to the system-managed thread pool, 
    /// - and exposes a method to get an OVERLAPPED* for asynchronous Win32 API calls which use OVERLAPPED I/O
    ///
    /// Basic usage:
    /// - construct a ThreadIocp object by passing in the HANDLE/SOCKET on which overlapped IO calls will be made
    /// - call new_request to get an OVERLAPPED* for an asynchronous Win32 API call the associated HANDLE/SOCKET
    ///   - additionally pass a function to be invoked on IO completion 
    /// - if the Win32 API succeeds or returns ERROR_IO_PENDING:
    ///    - the user's callback function will be called on completion [if succeeds or fails]
    ///    - from the callback function, the user then calls GetOverlappedResult/WSAGetOverlappedResult 
    ///      on the given OVERLAPPED* to get further details of the IO request [status, bytes transferred]
    /// - if the Win32 API fails with an error other than ERROR_IO_PENDING
    ///    - the user *must* call cancel_request, providing the OVERLAPPED* used in the failed API call
    ///    - that OVERLAPPED* is no longer valid and cannot be reused 
    ///      [new_request must be called again for another OVLERAPPED*]
    ///
    /// Additional notes regarding OVERLAPPED I/O:
    /// - the user must call new_request to get a new OVERLAPPED* before every Win32 API being made
    ///   - an OVERLAPPED* is valid only for that one API call and is invalid once the corresponding callback completes
    /// - if the IO call must be canceled after is completed successfully or returned ERROR_IO_PENDING, 
    ///   the user should take care to call the appropriate API (CancelIo, CancelIoEx, CloseHandle, closesocket)
    ///   - the user should then expect the callback to be invoked for all IO requests on that HANDLE/SOCKET
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    class ThreadIocp {
    public:
        //
        // These c'tors can fail under low resources
        // - ntl::Exception (from the ThreadPool APIs)
        //
        explicit ThreadIocp(_In_ HANDLE _handle, _In_opt_ PTP_CALLBACK_ENVIRON _ptp_env = nullptr)
        {
            ptp_io = ::CreateThreadpoolIo(_handle, IoCompletionCallback, nullptr, _ptp_env);
            if (nullptr == ptp_io) {
                throw ntl::Exception(::GetLastError(), L"CreateThreadpoolIo", L"ntl::ThreadIocp::ThreadIocp", false);
            }
        }

        explicit ThreadIocp(_In_ SOCKET _socket, _In_opt_ PTP_CALLBACK_ENVIRON _ptp_env = nullptr)
        {
            ptp_io = ::CreateThreadpoolIo(reinterpret_cast<HANDLE>(_socket), IoCompletionCallback, nullptr, _ptp_env);
            if (nullptr == ptp_io) {
                throw ntl::Exception(::GetLastError(), L"CreateThreadpoolIo", L"ntl::ThreadIocp::ThreadIocp", false);
            }
        }
        ~ThreadIocp()
        {
            // wait for all callbacks
            ::WaitForThreadpoolIoCallbacks(this->ptp_io, FALSE);
            ::CloseThreadpoolIo(this->ptp_io);
        }
        //
        // new_request is expected to be called before each call to a Win32 function taking an OVLERAPPED*
        // - which the caller expects to have their std::function invoked with the following signature:
        //     void callback_function(OVERLAPPED* _overlapped)
        //
        // The OVERLAPPED* returned is always owned by the object - never by the caller
        // - the caller is expected to pass it directly to a Win32 API
        // - after the callback completes, the OVERLAPPED* is no longer valid
        //
        // If the API which is given the associated HANDLE/SOCKET + OVERLAPPED* succeeds or returns ERROR_IO_PENDING,
        // - the OVERLAPPED* is now in-use and should not be touched until it's passed to the user's callback function
        //   [unless the user needs to cancel the IO request with another Win32 API call like CancelIoEx or closesocket]
        //
        // If the API which is given the associated HANDLE/SOCKET + OVERLAPPED* fails with an error other than ERROR_IO_PENDING
        // - the caller should immediately call cancel_request() with the corresponding OVERLAPPED*
        //
        // Callers can invoke new_request to issue separate and concurrent IO over the same HANDLE/SOCKET
        // - each call will return a unique OVERLAPPED*
        // - the callback will be given the OVERLAPPED* matching the IO that completed
        //
        OVERLAPPED* new_request(std::function<void(OVERLAPPED*)> _callback)
        {
            // this can fail by throwing std::bad_alloc
            ThreadIocpCallbackInfo* new_callback = new ThreadIocpCallbackInfo(std::move(_callback));
            // once creating a new request succeeds, start the IO
            // - all below calls are no-fail calls
            ::StartThreadpoolIo(this->ptp_io);
            ::ZeroMemory(&new_callback->ov, sizeof OVERLAPPED);
            return &new_callback->ov;
        }
        //
        // This function should be called only if the Win32 API call which was given the OVERLAPPED* from new_request
        // - failed with an error other than ERROR_IO_PENDING
        //
        // *Note*
        // This function does *not* cancel the IO call (e.g. does not cancel the ReadFile or WSARecv request)
        // - it is only to notify the threadpool that there will not be any IO over the OVERLAPPED*
        //
        void cancel_request(OVERLAPPED* _pov) NOEXCEPT
        {
            ::CancelThreadpoolIo(this->ptp_io);
            ThreadIocpCallbackInfo* old_request = reinterpret_cast<ThreadIocpCallbackInfo*>(_pov);
            delete old_request;
        }

        //
        // No default c'tor - preventing zombie objects
        // No copy c'tors
        //
        ThreadIocp() = delete;
        ThreadIocp(const ThreadIocp&) = delete;
        ThreadIocp& operator=(const ThreadIocp&) = delete;

    private:
        PTP_IO ptp_io = nullptr;

        static void CALLBACK IoCompletionCallback(
            PTP_CALLBACK_INSTANCE /*_instance*/,
            PVOID /*_context*/,
            PVOID _overlapped,
            ULONG /*_ioresult*/,
            ULONG_PTR /*_numberofbytestransferred*/,
            PTP_IO /*_io*/)
        {
            // this code may look really odd 
            // the Win32 TP APIs eat stack overflow exceptions and reuses the thread for the next TP request
            // it is *not* expected that callers can/will harden their callback functions to be resilient to running out of stack at any momemnt
            // since we *do* hit this in stress, and we face ugly lock-related breaks since an SEH was swallowed while a callback held a lock, 
            // we're working really hard to break and never let TP swalling SEH exceptions
            EXCEPTION_POINTERS* exr = nullptr;
            __try {
                ThreadIocpCallbackInfo* _request = reinterpret_cast<ThreadIocpCallbackInfo*>(_overlapped);
                _request->callback(static_cast<OVERLAPPED*>(_overlapped));
                delete _request;
            }
            // ReSharper disable once CppAssignedValueIsNeverUsed (exr is used in the except handler)
            __except ((exr = GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER)
            {
                __try {
                    ::RaiseFailFastException(exr->ExceptionRecord, exr->ContextRecord, 0);
                }
#pragma warning(suppress: 6320) // not hiding exceptions: RaiseFailFastException is fatal - this creates a break to help debugging in some scenarios
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    __debugbreak();
                }
            }
        }
    };

} // namespace

