// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// CPP Headers
#include <wchar.h>
#include <string>
#include <memory>

// OS Headers
#include <Windows.h>
#include <Rpc.h>

// ntl headers
#include "ntlException.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// UUID manipulation functions
///
/// Notice all functions are in the ntl::Uuid namespace
///
/// *** USE OF THESE FUNCTIONS REQUIRES LINKING TO $(SDK_LIB_PATH)\Rpcrt4.lib ***
///
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


namespace ntl {
namespace Uuid {

struct wszUuidDeleter {
    void operator()(RPC_WSTR* wstrptr) const {
        if (wstrptr)
        {
            ::RpcStringFreeW(wstrptr);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// generate_uuid
///
/// Generates a new UUID in the form of a std::wstring in format "01234567-89ab-cdef-0123-456789abcdef"
///
/// Can throw ntl::Exception on failure from the underlying win32 generation function
/// Can throw std::bad_alloc
///
////////////////////////////////////////////////////////////////////////////////////////////////////
inline
std::wstring generate_uuid()
{
    UUID tempUuid;
    RPC_STATUS rpcStatus = ::UuidCreate(&tempUuid);
    if (rpcStatus != RPC_S_OK) {
        throw ntl::Exception(rpcStatus, L"::UuidCreate", L"ntl::Uuid::generate_uuid", false);
    }

    RPC_WSTR wszUuid = nullptr;
    rpcStatus = ::UuidToStringW(&tempUuid, &wszUuid);
    if (rpcStatus != RPC_S_OK) {
        throw ntl::Exception(rpcStatus, L"::UuidToString", L"ntl::Uuid::generate_uuid", false);
    }
    // Make sure we free the UUID string (even if the wstring construction throws)
    std::unique_ptr<RPC_WSTR, wszUuidDeleter> handle(&wszUuid);

    return std::wstring(reinterpret_cast<wchar_t*>(wszUuid));
}

inline
std::wstring uuid_to_string(_In_ UUID _guid)
{
    RPC_WSTR wszUuid = nullptr;
    RPC_STATUS rpcStatus = ::UuidToStringW(&_guid, &wszUuid);
    if (rpcStatus != RPC_S_OK) {
        throw ntl::Exception(rpcStatus, L"::UuidToString", L"ntl::Uuid::uuid_to_string", false);
    }
    // Make sure we free the UUID string (even if the wstring construction throws)
    std::unique_ptr<RPC_WSTR, wszUuidDeleter> handle(&wszUuid);

    return std::wstring(reinterpret_cast<wchar_t*>(wszUuid));
}
inline
UUID string_to_uuid(_In_ LPCWSTR _guid)
{
    // must be a non-const string for the first argument
    UUID returned_uuid;
    std::wstring string_uuid(_guid);
    RPC_STATUS rpcStatus = ::UuidFromStringW(reinterpret_cast<RPC_WSTR>(&string_uuid[0]), &returned_uuid);
    if (rpcStatus != RPC_S_OK) {
        throw ntl::Exception(rpcStatus, L"::UuidFromStringW", L"ntl::Uuid::string_to_uuid", false);
    }

    return returned_uuid;
}

} // namespace Uuid
} // namespace ntl