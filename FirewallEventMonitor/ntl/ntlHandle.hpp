// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// os headers
#include <Windows.h>
#include <winsock2.h>
// project headers
#include "ntlVersionConversion.hpp"
#include "ntlScopedT.hpp"


namespace ntl {

    ///////////////////////////////////////////////////////////////////////////////////
    ///
    ///  typedef of ScopedT<T, Fn> using:
    ///  - a general Win32 HANDLE resource type
    ///  - a CloseHandle() function to close the HANDLE
    ///
    ///////////////////////////////////////////////////////////////////////////////////
    struct HandleDeleter {
        void operator() (const HANDLE h) const NOEXCEPT
        {
            if ((h != NULL) && (h != INVALID_HANDLE_VALUE)) {
                ::CloseHandle(h);
            }
        }
    };
    typedef ntl::ScopedT<HANDLE, NULL, HandleDeleter>  ScopedHandle;


    ///////////////////////////////////////////////////////////////////////////////////
    ///
    ///  typedef of ScopedT<T, Fn> using:
    ///  - a HKEY resource type (registry handles)
    ///  - a RegCloseKey() function to close the HANDLE
    ///
    ///////////////////////////////////////////////////////////////////////////////////
    struct HKeyDeleter {
        void operator() (const HKEY h) const NOEXCEPT
        {
            if ((h != NULL) &&
                (h != HKEY_CLASSES_ROOT) &&
                (h != HKEY_CURRENT_CONFIG) &&
                (h != HKEY_CURRENT_USER) &&
                (h != HKEY_LOCAL_MACHINE) &&
                (h != HKEY_USERS)) {
                // ignore the pre-defined HKEY values
                ::RegCloseKey(h);
            }
        }
    };
    typedef ntl::ScopedT<HKEY, NULL, HKeyDeleter>  ScopedHKey;


    ///////////////////////////////////////////////////////////////////////////////////
    ///
    ///  typedef of ntlSharedT<T, Fn> using:
    ///  - a HANDLE created with the FindFirst* APIs
    ///  - a FindClose() function to close the HANDLE
    ///
    ///////////////////////////////////////////////////////////////////////////////////
    struct FindHandleDeleter {
        void operator() (const HANDLE h) const NOEXCEPT
        {
            if ((h != NULL) && (h != INVALID_HANDLE_VALUE)) {
                ::FindClose(h);
            }
        }
    };
    typedef ntl::ScopedT<HANDLE, NULL, FindHandleDeleter>  ScopedFindHandle;


    ///////////////////////////////////////////////////////////////////////////////////
    ///
    ///  typedef of ntlSharedT<T, Fn> using:
    ///  - a HANDLE created with the OpenEventLog API
    ///  - a CloseEventLog() function to close the HANDLE
    ///
    ///////////////////////////////////////////////////////////////////////////////////
    struct EventLogHandleDeleter {
        void operator() (const HANDLE h) const NOEXCEPT
        {
            if ((h != NULL) && (h != INVALID_HANDLE_VALUE)) {
                ::CloseEventLog(h);
            }
        }
    };
    typedef ntl::ScopedT<HANDLE, NULL, EventLogHandleDeleter>  ScopedEventLogHandle;


    ///////////////////////////////////////////////////////////////////////////////////
    //
    ///  typedef of ntlSharedT<T, Fn> using:
    ///   - a HMODULE resource type
    ///   - a FreeLibrary() function to close the HMODULE
    ///
    ///////////////////////////////////////////////////////////////////////////////////
    struct LibraryHandleDeleter {
        void operator() (const HMODULE h) const NOEXCEPT
        {
            if (h != NULL) {
                ::FreeLibrary(h);
            }
        }
    };
    typedef ntl::ScopedT<HMODULE, NULL, LibraryHandleDeleter>  ScopedLibraryHandle;


    ///////////////////////////////////////////////////////////////////////////////////
    ///
    ///  typedef of ntlSharedT<T, Fn> using:
    ///  - a SC_HANDLE resource type
    ///  - a CloseServiceHandle() function to close the SC_HANDLE
    ///
    ///////////////////////////////////////////////////////////////////////////////////
    struct ServiceHandleDeleter {
        void operator() (const SC_HANDLE h) const NOEXCEPT
        {
            if (h != NULL) {
                ::CloseServiceHandle(h);
            }
        }
    };
    typedef ntl::ScopedT<SC_HANDLE, NULL, ServiceHandleDeleter>  ScopedServiceHandle;


    ///////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////
    ///
    ///  typedef of ntlSharedT<T, Fn> using:
    ///  - a SOCKET resource type
    ///  - a closesocket() function to close the HMODULE
    ///
    ///////////////////////////////////////////////////////////////////////////////////
    struct SocketHandleDeleter {
        void operator() (const SOCKET s) const NOEXCEPT
        {
            if (s != INVALID_SOCKET) {
                ::closesocket(s);
            }
        }
    };
    typedef ntl::ScopedT<SOCKET, INVALID_SOCKET, SocketHandleDeleter>  ScopedSocket;

}  // namespace ntl
