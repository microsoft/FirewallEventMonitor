// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// os headers
#include <Windows.h>
#include <winsock2.h>
#include <mswsock.h>
#include <rpc.h> // for GUID
// ntl headers
#include "ntlVersionConversion.hpp"
#include "ntlException.hpp"
#include "ntlScopeGuard.hpp"


namespace ntl {
    ///
    /// anonymous namespace to correctly hide this function to only this file
    /// - and thus avoid any ODR or name collision issues
    ///
    namespace {
        ///
        /// function pointers accessable from only this file (anonymous namespace)
        ///
        const unsigned fn_ptr_count = 9;
        LPFN_TRANSMITFILE            transmitfile = nullptr; // WSAID_TRANSMITFILE
        LPFN_ACCEPTEX                acceptex = nullptr; // WSAID_ACCEPTEX
        LPFN_GETACCEPTEXSOCKADDRS    getacceptexsockaddrs = nullptr; // WSAID_GETACCEPTEXSOCKADDRS
        LPFN_TRANSMITPACKETS         transmitpackets = nullptr; // WSAID_TRANSMITPACKETS
        LPFN_CONNECTEX               connentEx = nullptr; // WSAID_CONNECTEX
        LPFN_DISCONNECTEX            disconnentEx = nullptr; // WSAID_DISCONNECTEX
        LPFN_WSARECVMSG              wsarecvmsg = nullptr; // WSAID_WSARECVMSG
        LPFN_WSASENDMSG              wsasendmsg = nullptr; // WSAID_WSASENDMSG
        RIO_EXTENSION_FUNCTION_TABLE rioextensionfunctiontable = { 0 }; // WSAID_MULTIPLE_RIO

        ///
        /// SocketExtensionInit
        ///
        /// InitOnce function only to be called locally to ensure WSAStartup is held
        /// for the function pointers to remain accurate
        ///
        static INIT_ONCE s_SocketExtensionInitOnce = INIT_ONCE_STATIC_INIT;
        static BOOL CALLBACK s_SocketExtensionInitFn(_In_ PINIT_ONCE, _In_ PVOID perror, _In_ PVOID*)
        {
            WSADATA wsadata;
            int wsError = ::WSAStartup(WINSOCK_VERSION, &wsadata);
            if (wsError != 0) {
                *static_cast<int*>(perror) = wsError;
                return FALSE;
            }
            ScopeGuard(WSACleanupOnExit, { ::WSACleanup(); });

            // check to see if need to create a temp socket
            SOCKET local_socket = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            if (INVALID_SOCKET == local_socket) {
                *static_cast<int*>(perror) = ::WSAGetLastError();
                return FALSE;
            }
            ScopeGuard(closesocketOnExit, { ::closesocket(local_socket); });

            // control code and the size to fetch the extension function pointers
            for (unsigned fn_loop = 0; fn_loop < fn_ptr_count; ++fn_loop) {

                VOID*  function_ptr = nullptr;
                DWORD controlCode = SIO_GET_EXTENSION_FUNCTION_POINTER;
                DWORD bytes = static_cast<DWORD>(sizeof(VOID*));
                // must declare GUID explicitly at a global scope as some commonly used test libraries
                // - incorrectly pull it into their own namespace
                ::GUID guid = { 0 };

                switch (fn_loop) {
                    case 0: {
                        function_ptr = reinterpret_cast<VOID*>(&transmitfile);
                        ::GUID tmp_guid = WSAID_TRANSMITFILE;
                        ::memcpy(&guid, &tmp_guid, sizeof ::GUID);
                        break;
                    }
                    case 1: {
                        function_ptr = reinterpret_cast<VOID*>(&acceptex);
                        ::GUID tmp_guid = WSAID_ACCEPTEX;
                        ::memcpy(&guid, &tmp_guid, sizeof ::GUID);
                        break;
                    }
                    case 2: {
                        function_ptr = reinterpret_cast<VOID*>(&getacceptexsockaddrs);
                        ::GUID tmp_guid = WSAID_GETACCEPTEXSOCKADDRS;
                        ::memcpy(&guid, &tmp_guid, sizeof ::GUID);
                        break;
                    }
                    case 3: {
                        function_ptr = reinterpret_cast<VOID*>(&transmitpackets);
                        ::GUID tmp_guid = WSAID_TRANSMITPACKETS;
                        ::memcpy(&guid, &tmp_guid, sizeof ::GUID);
                        break;
                    }
                    case 4: {
                        function_ptr = reinterpret_cast<VOID*>(&connentEx);
                        ::GUID tmp_guid = WSAID_CONNECTEX;
                        ::memcpy(&guid, &tmp_guid, sizeof ::GUID);
                        break;
                    }
                    case 5: {
                        function_ptr = reinterpret_cast<VOID*>(&disconnentEx);
                        ::GUID tmp_guid = WSAID_DISCONNECTEX;
                        ::memcpy(&guid, &tmp_guid, sizeof ::GUID);
                        break;
                    }
                    case 6: {
                        function_ptr = reinterpret_cast<VOID*>(&wsarecvmsg);
                        ::GUID tmp_guid = WSAID_WSARECVMSG;
                        ::memcpy(&guid, &tmp_guid, sizeof ::GUID);
                        break;
                    }
                    case 7: {
                        function_ptr = reinterpret_cast<VOID*>(&wsasendmsg);
                        ::GUID tmp_guid = WSAID_WSASENDMSG;
                        ::memcpy(&guid, &tmp_guid, sizeof ::GUID);
                        break;
                    }
                    case 8: {
                        function_ptr = reinterpret_cast<VOID*>(&rioextensionfunctiontable);
                        ::GUID tmp_guid = WSAID_MULTIPLE_RIO;
                        ::memcpy(&guid, &tmp_guid, sizeof ::GUID);
                        controlCode = SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER;
                        bytes = static_cast<DWORD>(sizeof(rioextensionfunctiontable));
                        ::ZeroMemory(&rioextensionfunctiontable, bytes);
                        rioextensionfunctiontable.cbSize = bytes;
                        break;
                    }
                }

                if (0 != ::WSAIoctl(
                    local_socket,
                    controlCode,
                    &guid,
                    static_cast<DWORD>(sizeof(guid)),
                    function_ptr,
                    bytes,
                    &bytes,
                    nullptr, // lpOverlapped
                    nullptr  // lpCompletionRoutine
                   )) 
                {
                    DWORD errorCode = ::WSAGetLastError();
                    if (8 == fn_loop && errorCode == WSAEOPNOTSUPP) {
                        // ignore not-supported errors for RIO APIs to support Win7
                    } else {
                        *static_cast<int*>(perror) = errorCode;
                        return FALSE;
                    }
                }
            }

            return TRUE;
        }
        static void s_InitSocketExtensions()
        {
            DWORD error = 0;
            if (!::InitOnceExecuteOnce(&s_SocketExtensionInitOnce, s_SocketExtensionInitFn, &error, nullptr)) {
                throw ntl::Exception(error, L"ntl::SocketExtensions", false);
            }
        }
    }; // anonymous namespace

    ///
    /// Dynamic check if RIO is available on this operating system
    ///
    inline bool SocketIsRioAvailable()
    {
        s_InitSocketExtensions();
        return (nullptr != rioextensionfunctiontable.RIOReceive);
    }

    ///
    /// TransmitFile
    ///
    inline BOOL TransmitFile(
        _In_ SOCKET hSocket,
        _In_ HANDLE hFile,
        const DWORD nNumberOfBytesToWrite,
        const DWORD nNumberOfBytesPerSend,
        _Inout_opt_ LPOVERLAPPED lpOverlapped,
        _In_opt_ LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers,
        const  DWORD dwReserved
       )
    {
        s_InitSocketExtensions();
        return transmitfile(
            hSocket,
            hFile,
            nNumberOfBytesToWrite,
            nNumberOfBytesPerSend,
            lpOverlapped,
            lpTransmitBuffers,
            dwReserved
           );
    }

    ///
    /// TransmitPackets
    ///
    inline BOOL TransmitPackets(
        _In_ SOCKET hSocket,
        _In_opt_ LPTRANSMIT_PACKETS_ELEMENT lpPacketArray,
        const DWORD nElementCount,
        const DWORD nSendSize,
        _Inout_opt_ LPOVERLAPPED lpOverlapped,
        const DWORD dwFlags
       )
    {
        s_InitSocketExtensions();
        return transmitpackets(
            hSocket,
            lpPacketArray,
            nElementCount,
            nSendSize,
            lpOverlapped,
            dwFlags
           );
    }

    ///
    /// AcceptEx
    ///
    inline BOOL ntlAcceptEx(
        _In_ SOCKET sListenSocket,
        _In_ SOCKET sAcceptSocket,
        _Out_writes_bytes_(dwReceiveDataLength + dwLocalAddressLength + dwRemoteAddressLength) PVOID lpOutputBuffer,
        const DWORD dwReceiveDataLength,
        const DWORD dwLocalAddressLength,
        const DWORD dwRemoteAddressLength,
        _Out_ LPDWORD lpdwBytesReceived,
        _Inout_ LPOVERLAPPED lpOverlapped
       )
    {
        s_InitSocketExtensions();
        return acceptex(
            sListenSocket,
            sAcceptSocket,
            lpOutputBuffer,
            dwReceiveDataLength,
            dwLocalAddressLength,
            dwRemoteAddressLength,
            lpdwBytesReceived,
            lpOverlapped
           );
    }

    ///
    /// GetAcceptExSockaddrs
    ///
    inline VOID GetAcceptExSockaddrs(
        _In_reads_bytes_(dwReceiveDataLength + dwLocalAddressLength + dwRemoteAddressLength) PVOID lpOutputBuffer,
        const DWORD dwReceiveDataLength,
        const DWORD dwLocalAddressLength,
        const DWORD dwRemoteAddressLength,
        _Outptr_result_bytebuffer_(*LocalSockaddrLength) struct sockaddr **LocalSockaddr,
        _Out_ LPINT LocalSockaddrLength,
        _Outptr_result_bytebuffer_(*RemoteSockaddrLength) struct sockaddr **RemoteSockaddr,
        _Out_ LPINT RemoteSockaddrLength
       )
    {
        s_InitSocketExtensions();
        return getacceptexsockaddrs(
            lpOutputBuffer,
            dwReceiveDataLength,
            dwLocalAddressLength,
            dwRemoteAddressLength,
            LocalSockaddr,
            LocalSockaddrLength,
            RemoteSockaddr,
            RemoteSockaddrLength
           );
    }

    ///
    /// ConnectEx
    ///
    inline BOOL ConnectEx(
        _In_ SOCKET s,
        _In_reads_bytes_(namelen) const struct sockaddr FAR *name,
        const int namelen,
        _In_reads_bytes_opt_(dwSendDataLength) PVOID lpSendBuffer,
        const DWORD dwSendDataLength,
        _When_(lpSendBuffer, _Out_) LPDWORD lpdwBytesSent, // optional if lpSendBuffer is null
        _Inout_ LPOVERLAPPED lpOverlapped
       )
    {
        s_InitSocketExtensions();
        return connentEx(
            s,
            name,
            namelen,
            lpSendBuffer,
            dwSendDataLength,
            lpdwBytesSent,
            lpOverlapped
           );
    }

    ///
    /// DisconnectEx
    ///
    inline BOOL ntlDisconnectEx(
        _In_ SOCKET s,
        _Inout_opt_ LPOVERLAPPED lpOverlapped,
        const DWORD  dwFlags,
        const DWORD  dwReserved
       )
    {
        s_InitSocketExtensions();
        return disconnentEx(
            s,
            lpOverlapped,
            dwFlags,
            dwReserved
           );
    }

    ///
    /// WSARecvMsg
    ///
    inline INT WSARecvMsg(
        _In_ SOCKET s,
        _Inout_ LPWSAMSG lpMsg,
        _Out_opt_ LPDWORD lpdwNumberOfBytesRecvd,
        _Inout_opt_ LPWSAOVERLAPPED lpOverlapped,
        _In_opt_ LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
       )
    {
        s_InitSocketExtensions();
        return wsarecvmsg(
            s,
            lpMsg,
            lpdwNumberOfBytesRecvd,
            lpOverlapped,
            lpCompletionRoutine
           );
    }

    ///
    /// WSASendMsg
    ///
    inline INT WSASendMsg(
        _In_ SOCKET s,
        _In_ LPWSAMSG lpMsg,
        const DWORD dwFlags,
        _Out_opt_ LPDWORD lpNumberOfBytesSent,
        _Inout_opt_ LPWSAOVERLAPPED lpOverlapped,
        _In_opt_ LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
       )
    {
        s_InitSocketExtensions();
        return wsasendmsg(
            s,
            lpMsg,
            dwFlags,
            lpNumberOfBytesSent,
            lpOverlapped,
            lpCompletionRoutine
           );
    }

    ///
    /// RioReceive 
    ///
    inline BOOL RIOReceive(
        _In_ RIO_RQ socketQueue,
        _In_reads_(dataBufferCount) PRIO_BUF pData,
        const ULONG dataBufferCount,
        const DWORD dwFlags,
        _In_ PVOID requestContext
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIOReceive(
            socketQueue,
            pData,
            dataBufferCount,
            dwFlags,
            requestContext
           );
    }

    ///
    /// RioReceiveEx 
    ///
    inline int RIOReceiveEx(
        _In_ RIO_RQ socketQueue,
        _In_reads_(dataBufferCount) PRIO_BUF pData,
        const ULONG dataBufferCount,
        _In_opt_ PRIO_BUF pLocalAddress,
        _In_opt_ PRIO_BUF pRemoteAddress,
        _In_opt_ PRIO_BUF pControlContext,
        _In_opt_ PRIO_BUF pdwFlags,
        const DWORD dwFlags,
        _In_ PVOID requestContext
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIOReceiveEx(
            socketQueue,
            pData,
            dataBufferCount,
            pLocalAddress,
            pRemoteAddress,
            pControlContext,
            pdwFlags,
            dwFlags,
            requestContext
           );
    }

    ///
    /// RioSend
    ///
    inline BOOL RIOSend(
        _In_ RIO_RQ socketQueue,
        _In_reads_(dataBufferCount) PRIO_BUF pData,
        const ULONG dataBufferCount,
        const DWORD dwFlags,
        _In_ PVOID requestContext
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIOSend(
            socketQueue,
            pData,
            dataBufferCount,
            dwFlags,
            requestContext
           );
    }

    ///
    /// RioSendEx
    ///
    inline BOOL RIOSendEx(
        _In_ RIO_RQ socketQueue,
        _In_reads_(dataBufferCount) PRIO_BUF pData,
        const ULONG dataBufferCount,
        _In_opt_ PRIO_BUF pLocalAddress,
        _In_opt_ PRIO_BUF pRemoteAddress,
        _In_opt_ PRIO_BUF pControlContext,
        _In_opt_ PRIO_BUF pdwFlags,
        const DWORD dwFlags,
        _In_ PVOID requestContext
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIOSendEx(
            socketQueue,
            pData,
            dataBufferCount,
            pLocalAddress,
            pRemoteAddress,
            pControlContext,
            pdwFlags,
            dwFlags,
            requestContext
           );
    }

    ///
    /// RioCloseCompletionQueue
    ///
    inline void RIOCloseCompletionQueue(
        _In_ RIO_CQ cq
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIOCloseCompletionQueue(
            cq
           );
    }

    ///
    /// RioCreateCompletionQueue
    ///
    inline RIO_CQ RIOCreateCompletionQueue(
        const DWORD queueSize,
        _In_opt_ PRIO_NOTIFICATION_COMPLETION pNotificationCompletion
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIOCreateCompletionQueue(
            queueSize,
            pNotificationCompletion
           );
    }

    ///
    /// RioCreateRequestQueue
    ///
    inline RIO_RQ RIOCreateRequestQueue(
        _In_ SOCKET socket,
        const ULONG maxOutstandingReceive,
        const ULONG maxReceiveDataBuffers,
        const ULONG maxOutstandingSend,
        const ULONG maxSendDataBuffers,
        _In_ RIO_CQ receiveCq,
        _In_ RIO_CQ sendCq,
        _In_ PVOID socketContext
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIOCreateRequestQueue(
            socket,
            maxOutstandingReceive,
            maxReceiveDataBuffers,
            maxOutstandingSend,
            maxSendDataBuffers,
            receiveCq,
            sendCq,
            socketContext
           );
    }

    ///
    /// RioDequeueCompletion
    ///
    inline ULONG RIODequeueCompletion(
        _In_ RIO_CQ cq,
        _Out_writes_to_(arraySize, return) PRIORESULT array,
        const ULONG arraySize
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIODequeueCompletion(
            cq,
            array,
            arraySize
           );
    }

    ///
    /// RioDeregisterBuffer
    ///
    inline void RIODeregisterBuffer(
        _In_ RIO_BUFFERID bufferId
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIODeregisterBuffer(
            bufferId
           );
    }

    ///
    /// RioNotify
    ///
    inline int RIONotify(
        _In_ RIO_CQ cq
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIONotify(
            cq
           );
    }

    ///
    /// RioRegisterBuffer 
    ///
    inline RIO_BUFFERID RIORegisterBuffer(
        _In_ PCHAR dataBuffer,
        const DWORD dataLength
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIORegisterBuffer(
            dataBuffer,
            dataLength
           );
    }

    ///
    /// RioResizeCompletionQueue
    ///
    inline BOOL RIOResizeCompletionQueue(
        _In_ RIO_CQ cq,
        const DWORD queueSize
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIOResizeCompletionQueue(
            cq,
            queueSize
           );
    }

    ///
    /// RioResizeRequestQueue
    ///
    inline BOOL RIOResizeRequestQueue(
        _In_ RIO_RQ rq,
        const DWORD maxOutstandingReceive,
        const DWORD maxOutstandingSend
       )
    {
        s_InitSocketExtensions();
        return rioextensionfunctiontable.RIOResizeRequestQueue(
            rq,
            maxOutstandingReceive,
            maxOutstandingSend
           );
    }

} // namespace ntl

