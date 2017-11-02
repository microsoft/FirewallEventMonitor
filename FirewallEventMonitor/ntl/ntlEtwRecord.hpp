// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// CPP Headers
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <algorithm>
#include <utility>

// OS Headers
#include <Windows.h>
#include <winsock2.h>
#include <Rpc.h>
// including these to get IP helper functions
#include <WS2TCPIP.H>
#include <mstcpip.h>
// these 3 headers needed for evntrace.h
#include <wmistr.h>
#include <winmeta.h>
#include <evntcons.h>
#include <evntrace.h>
#include <Tdh.h>
#include <Sddl.h>
// assert
#include <assert.h>

// Local headers
#include "ntlException.hpp"
#include "ntlString.hpp"
#include "ntlUuid.hpp"

namespace ntl
{

struct WCharDeleter {
    void operator()(WCHAR* wcharptr) NOEXCEPT {
        if (wcharptr)
        {
            ::LocalFree(wcharptr);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
//  class EtwRecord
//
//  Encapsulates accessing all the various properties one can potentially
//      gather from a EVENT_RECORD structure passed to the consumer from ETW.
//
//  The constructor takes a ptr to the EVENT_RECORD to encapsulate, and makes
//      a deep copy of all embedded and referenced data structures to access
//      with the   getter member functions.
//
//  There are 2 method-types exposed:    get*() and  query*()
//        get* functions have no parameters, and always return the associated
//          value.  They will always have a value to return from any event.
//
//       query* functions take applicable [out] as parameters, and return bool.
//          The values they retrieve are not guaranteed to be in all events,
//          and will return false if they don't exist in the encapsulated event.
//
//  Note that all returned strings are dynamically allocated and returned via
//      std::vector<wchar_t> smart pointer objects.  Include std::vector.h
//      to use this class.  The class manages the lifetime of the buffer, and 
//      will ensure it's deleted when the final object is destructed.  Note that
//      copying the object is very cheap (one Interlocked operation) and cannot
//      fail. (thus isn't highly suggested to copy these smart pointer objects
//      by value, not by reference - which defeats the internal refcounting)
//
//  All methods that do not have a NOEXCEPT specification can throw
//      std::bad_alloc - which is derived from std::exception.  The constructor 
//      can throw ntl::Exception, which is also derived from std::exception.
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
    
    
class EtwRecord
{
public:
    /////////////////////////////////////////////////////////////
    //
    // A public typedef to access the pair class containing the property data
    //
    /////////////////////////////////////////////////////////////
    typedef struct std::pair<std::vector<BYTE>, ULONG> PropertyPair;

public:
    /////////////////////////////////////////////////////////////
    //
    // Constructors
    //  - default
    //  - specifying the EVENT_RECORD* to deep-copy
    //
    // Destructor
    // Copy Constructor
    // Copy Assignment operator
    //
    // Assignment operator taking an EVENT_RECORD*
    //  - replaces the existing encapsulated EVENT_RECORD info
    //    with the specified EVENT_RECORD.
    //
    /////////////////////////////////////////////////////////////
    EtwRecord() NOEXCEPT;
    EtwRecord(_In_ PEVENT_RECORD);
    ~EtwRecord() NOEXCEPT;
    EtwRecord(const EtwRecord&) NOEXCEPT;
    EtwRecord& operator=(const EtwRecord&) NOEXCEPT;
    EtwRecord& operator=(_In_ PEVENT_RECORD);
    /////////////////////////////////////////////////////////////
    //
    // Implementing swap() to be a friendly container
    //
    /////////////////////////////////////////////////////////////
    void swap(EtwRecord&) NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // Printing the entire ETW record
    // Printing just the formatted event message
    // - optionally with full details of each property
    //
    /////////////////////////////////////////////////////////////
    void writeRecord(std::wstring&) const;
    std::wstring writeRecord() const {
        std::wstring wsRecord;
        writeRecord(wsRecord);
        return wsRecord;
    }
    void writeFormattedMessage(std::wstring&, bool) const;
    std::wstring writeFormattedMessage(bool _details) const {
        std::wstring wsRecord;
        writeFormattedMessage(wsRecord, _details);
        return wsRecord;
    }
    /////////////////////////////////////////////////////////////
    //
    // comparison operators
    //
    /////////////////////////////////////////////////////////////
    bool operator==(const EtwRecord&) const;
    bool operator!=(const EtwRecord&) const;
    /////////////////////////////////////////////////////////////
    //
    // EVENT_HEADER fields (8)
    //
    /////////////////////////////////////////////////////////////
    ULONG  getThreadId() const NOEXCEPT;
    ULONG  getProcessId() const NOEXCEPT;
    LARGE_INTEGER  getTimeStamp() const NOEXCEPT;
    GUID   getProviderId() const NOEXCEPT;
    GUID   getActivityId() const NOEXCEPT;
    _Success_(return)
    bool  queryKernelTime(_Out_ ULONG *) const NOEXCEPT;
    _Success_(return)
    bool  queryUserTime(_Out_ ULONG *) const NOEXCEPT;
    ULONG64  getProcessorTime() const NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // EVENT_DESCRIPTOR fields (7)
    //
    /////////////////////////////////////////////////////////////
    USHORT  getEventId() const NOEXCEPT;
    UCHAR   getVersion() const NOEXCEPT;
    UCHAR   getChannel() const NOEXCEPT;
    UCHAR   getLevel() const NOEXCEPT;
    UCHAR   getOpcode() const NOEXCEPT;
    USHORT  getTask() const NOEXCEPT;
    ULONGLONG  getKeyword() const NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // ETW_BUFFER_CONTEXT fields (3)
    //
    /////////////////////////////////////////////////////////////
    UCHAR   getProcessorNumber() const NOEXCEPT;
    UCHAR   getAlignment() const NOEXCEPT;
    USHORT  getLoggerId() const NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // EVENT_HEADER_EXTENDED_DATA_ITEM options (6)
    //
    /////////////////////////////////////////////////////////////
    _Success_(return)
    bool  queryRelatedActivityId(_Out_ GUID *) const NOEXCEPT;
    _Success_(return)
    bool  querySID(_Out_ std::vector<BYTE>& , _Out_ size_t*) const;
    _Success_(return)
    bool  queryTerminalSessionId(_Out_ ULONG *) const NOEXCEPT;
    _Success_(return)
    bool  queryTransactionInstanceId(_Out_ ULONG *) const NOEXCEPT;
    _Success_(return)
    bool  queryTransactionParentInstanceId(_Out_ ULONG *) const NOEXCEPT;
    _Success_(return)
    bool  queryTransactionParentGuid(_Out_ GUID *) const NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // TRACE_EVENT_INFO options (16)
    //
    /////////////////////////////////////////////////////////////
    _Success_(return)
    bool  queryProviderGuid(_Out_ GUID *) const NOEXCEPT;
    _Success_(return)
    bool  queryDecodingSource(_Out_ DECODING_SOURCE *) const NOEXCEPT;
    _Success_(return)
    bool  queryProviderName(_Out_ std::wstring&) const;
    _Success_(return)
    bool  queryLevelName(_Out_ std::wstring&) const;
    _Success_(return)
    bool  queryChannelName(_Out_ std::wstring&) const;
    _Success_(return)
    bool  queryKeywords(_Out_ std::vector<std::wstring> &) const;
    _Success_(return)
    bool  queryTaskName(_Out_ std::wstring&) const;
    _Success_(return)
    bool  queryOpcodeName(_Out_ std::wstring&) const;
    _Success_(return)
    bool  queryEventMessage(_Out_ std::wstring&) const;
    _Success_(return)
    bool  queryProviderMessageName(_Out_ std::wstring&) const;
    _Success_(return)
    bool  queryPropertyCount(_Out_ ULONG *) const NOEXCEPT;
    _Success_(return)
    bool  queryTopLevelPropertyCount(_Out_ ULONG *) const NOEXCEPT;
    _Success_(return)
    bool  queryEventPropertyStringValue(_Out_ std::wstring&) const;
    _Success_(return)
    bool  queryEventPropertyName(const unsigned long ulIndex, _Out_ std::wstring& out_wsPropertyName) const;
    _Success_(return)
    bool  queryEventProperty(_In_z_ const wchar_t*, _Out_ std::wstring&) const;
    _Success_(return)
    bool  queryEventProperty(_In_z_ const wchar_t*, _Out_ PropertyPair&) const;
    _Success_(return)
    bool  queryEventProperty(const unsigned long, _Out_ std::wstring&) const;

private:
    //
    // private method to build a formatted string from the specified property offset
    //
    std::wstring buildEventPropertyString(ULONG) const;
    //
    // eventHeader and etwBufferContext are just shallow-copies
    //      of the the EVENT_HEADER and ETW_BUFFER_CONTEXT structs.
    //
    EVENT_HEADER eventHeader;
    ETW_BUFFER_CONTEXT etwBufferContext;
    //
    // v_eventHeaderExtendedData and v_pEventHeaderData stores a deep-copy
    //      of the the EVENT_HEADER_EXTENDED_DATA_ITEM struct.
    //
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM> v_eventHeaderExtendedData;
    std::vector<std::vector<BYTE>> v_pEventHeaderData;
    //
    // ptraceEventInfo stores a deep copy of the TRACE_EVENT_INFO struct.
    //
    std::vector<BYTE> ptraceEventInfo;
    ULONG cbtraceEventInfo;
    //
    // vPropertyInfo stores an array of all properties
    //
    std::vector<PropertyPair> vtraceProperties;
    typedef struct std::pair<std::vector<WCHAR>, ULONG> ntlMappingPair;
    std::vector<ntlMappingPair> vtraceMapping;
    //
    // need to allow a default empty c'tor, so must track initialization status
    //
    bool bInit;
};
    
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  Default Constructor
//
////////////////////////////////////////////////////////////////////////////////
inline
EtwRecord::EtwRecord() NOEXCEPT
{
    ::ZeroMemory(&eventHeader, sizeof(EVENT_HEADER));
    ::ZeroMemory(&etwBufferContext, sizeof(ETW_BUFFER_CONTEXT));
    cbtraceEventInfo = 0;
    bInit = false;
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  Constructor taking/copying an EVENT_RECORD*
//
////////////////////////////////////////////////////////////////////////////////
inline
EtwRecord::EtwRecord(_In_ PEVENT_RECORD in_pRecord)
: eventHeader(in_pRecord->EventHeader),
  etwBufferContext(in_pRecord->BufferContext),
  v_eventHeaderExtendedData(),
  v_pEventHeaderData(),
  ptraceEventInfo(),
  cbtraceEventInfo(0),
  vtraceProperties(),
  bInit(false)
{
    if (in_pRecord->ExtendedDataCount > 0)
    {
        // Copying the EVENT_HEADER_EXTENDED_DATA_ITEM requires a deep-copy its data buffer
        //    and to point the local struct at the locally allocated and copied buffer
        //    since we won't have direct access to the original buffer later
        v_eventHeaderExtendedData.resize(in_pRecord->ExtendedDataCount);
        v_pEventHeaderData.resize(in_pRecord->ExtendedDataCount);
            
        for (unsigned uCount = 0; uCount < v_eventHeaderExtendedData.size(); ++uCount)
        {
            PEVENT_HEADER_EXTENDED_DATA_ITEM ptempItem = in_pRecord->ExtendedData;
            ptempItem += uCount;
                
            std::vector<BYTE> ptempBytes = std::vector<BYTE>(ptempItem->DataSize);
            memcpy_s(
                ptempBytes.data(),
                ptempItem->DataSize,
                reinterpret_cast<BYTE*>(ptempItem->DataPtr),
                ptempItem->DataSize
               );
                
            v_pEventHeaderData[uCount] = std::move(ptempBytes);
            v_eventHeaderExtendedData[uCount] = *ptempItem;
            v_eventHeaderExtendedData[uCount].DataPtr = reinterpret_cast<ULONGLONG>(0ULL + v_pEventHeaderData[uCount].data());
        }
    }
    
    if (eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY)
    {
        cbtraceEventInfo = in_pRecord->UserDataLength;
        ptraceEventInfo.resize(cbtraceEventInfo);
        memcpy_s(
            ptraceEventInfo.data(),
            cbtraceEventInfo,
            in_pRecord->UserData,
            cbtraceEventInfo
           );
    }
    else
    {
        cbtraceEventInfo = 0;
        ULONG ulret = ::TdhGetEventInformation(
            in_pRecord,
            0,
            NULL,
            NULL,
            &cbtraceEventInfo
           );
        if (ERROR_INSUFFICIENT_BUFFER == ulret)
        {
            ptraceEventInfo.resize(cbtraceEventInfo);
            ulret = ::TdhGetEventInformation(
                in_pRecord,
                0,
                NULL,
                reinterpret_cast<PTRACE_EVENT_INFO>(ptraceEventInfo.data()),
                &cbtraceEventInfo
               );
        }
        if (ulret != ERROR_SUCCESS)
        {
            throw ntl::Exception(ulret, L"TdhGetEventInformation", L"EtwRecord::EtwRecord", false);
        }
        //
        // retrieve all property data points - need to do this in the c'tor since the original EVENT_RECORD is required
        //
        BYTE* pByteInfo = this->ptraceEventInfo.data();
        TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
        unsigned long total_properties = pTraceInfo->TopLevelPropertyCount;
        if (total_properties > 0)
        {
            //
            // variables for TdhFormatProperty
            //
            USHORT UserDataLength = in_pRecord->UserDataLength;
            PBYTE UserData = static_cast<PBYTE>(in_pRecord->UserData);
            //
            // go through event event, and pull out the necessary data
            //
            for (unsigned long property_count = 0; property_count < total_properties; ++property_count)
            {
                if (pTraceInfo->EventPropertyInfoArray[property_count].Flags & PropertyStruct)
                {
                    //
                    // currently not supporting deep-copying event data of structs
                    //
                    ::OutputDebugStringW(
                        ntl::String::format_string(
                            L"EtwRecord cannot support a PropertyStruct : provider %s, property %s, event id %u",
                            ntl::Uuid::uuid_to_string(eventHeader.ProviderId).c_str(),
                            reinterpret_cast<wchar_t*>(pByteInfo + pTraceInfo->EventPropertyInfoArray[property_count].NameOffset),
                            eventHeader.EventDescriptor.Id).c_str());
#ifdef NTL_TDHFORMAT_FATALCONDITION
                    ::DebugBreak();
#endif
                    this->vtraceMapping.push_back(std::make_pair(std::vector<WCHAR>(), 0));
                    this->vtraceProperties.push_back(std::make_pair(std::vector<BYTE>(), 0));
                }
                else if (pTraceInfo->EventPropertyInfoArray[property_count].count > 1)
                {
                    //
                    // currently not supporting deep-copying event data of arrays
                    //
                    ::OutputDebugStringW(
                        ntl::String::format_string(
                            L"EtwRecord cannot support an array property size %u : provider %s, property %s, event id %u",
                            pTraceInfo->EventPropertyInfoArray[property_count].count,
                            ntl::Uuid::uuid_to_string(eventHeader.ProviderId).c_str(),
                            reinterpret_cast<wchar_t*>(pByteInfo + pTraceInfo->EventPropertyInfoArray[property_count].NameOffset),
                            eventHeader.EventDescriptor.Id).c_str());
#ifdef NTL_TDHFORMAT_FATALCONDITION
                    ::DebugBreak();
#endif
                    this->vtraceMapping.push_back(std::make_pair(std::vector<WCHAR>(), 0));
                    this->vtraceProperties.push_back(std::make_pair(std::vector<BYTE>(), 0));
                }
                else
                {
                    //
                    // define the event we want with a PROPERTY_DATA_DESCRIPTOR
                    //
                    PROPERTY_DATA_DESCRIPTOR dataDescriptor;
                    dataDescriptor.PropertyName =  reinterpret_cast<ULONGLONG>(pByteInfo + pTraceInfo->EventPropertyInfoArray[property_count].NameOffset);
                    dataDescriptor.ArrayIndex = ULONG_MAX;
                    dataDescriptor.Reserved = 0UL;
                    //
                    // get the buffer size first
                    //
                    ULONG cbPropertyData = 0;
                    ulret = ::TdhGetPropertySize(
                        in_pRecord,
                        0,    // not using WPP or 'classic' ETW
                        NULL, // not using WPP or 'classic' ETW
                        1,    // one property at a time - not support structs of data at this time
                        &dataDescriptor,
                        &cbPropertyData
                       );
                    if (ulret != ERROR_SUCCESS)
                    {
                        throw ntl::Exception(ulret, L"TdhGetPropertySize", L"EtwRecord::EtwRecord", false);
                    }
                    //
                    // now allocate the required buffer, and copy the data
                    // - only if the buffer size > 0
                    //
                    std::vector<BYTE> pPropertyData;
                    if (cbPropertyData > 0)
                    {
                        pPropertyData.resize(cbPropertyData);
                        ulret = ::TdhGetProperty(
                            in_pRecord,
                            0,    // not using WPP or 'classic' ETW
                            NULL, // not using WPP or 'classic' ETW
                            1,    // one property at a time - not support structs of data at this time
                            &dataDescriptor,
                            cbPropertyData,
                            pPropertyData.data()
                           );
                        if (ulret != ERROR_SUCCESS)
                        {
                            throw ntl::Exception(ulret, L"TdhGetProperty", L"EtwRecord::EtwRecord", false);
                        }
                    }
                    this->vtraceProperties.push_back(std::make_pair(std::move(pPropertyData), cbPropertyData));

                    //
                    // additionally capture the mapped string for the property, if it exists
                    //
                    DWORD dwMapInfoSize = 0;
                    std::vector<BYTE> pPropertyMap;
                    PWSTR szMapName = reinterpret_cast<PWSTR>(pByteInfo + pTraceInfo->EventPropertyInfoArray[property_count].nonStructType.MapNameOffset);
                    // first query the size needed
                    ulret = ::TdhGetEventMapInformation(
                        in_pRecord, 
                        szMapName,
                        NULL, 
                        &dwMapInfoSize
                       );
                    if (ERROR_INSUFFICIENT_BUFFER == ulret)
                    {
                        pPropertyMap.resize(dwMapInfoSize);
                        ulret = ::TdhGetEventMapInformation(
                            in_pRecord, 
                            szMapName, 
                            reinterpret_cast<PEVENT_MAP_INFO>(pPropertyMap.data()), 
                            &dwMapInfoSize
                           );
                    }
                    switch (ulret)
                    {
                    case ERROR_SUCCESS:
                        // all good - do nothing
                        break;
                    case ERROR_NOT_FOUND:
                        // this is OK to keep this event - there just wasn't a mapping for a formatted string
                        pPropertyMap.clear();
                        break;
                    default:
                        // any other error is an unexpected failure
#ifdef NTL_TDHFORMAT_FATALCONDITION
                        AlwaysFatalCondition(
                            ntl::String::format_string(
                                L"TdhGetEventMapInformation failed with error %u, EVENT_RECORD %p, TRACE_EVENT_INFO %p",
                                ulret, in_pRecord, pTraceInfo).c_str());
#endif
                        pPropertyMap.clear();
                    }
                    //
                    // if we successfully retreived the property info
                    // format the mapped property value
                    //
                    if (pPropertyMap.data() != nullptr)
                    {
                        USHORT property_length = pTraceInfo->EventPropertyInfoArray[property_count].length;
                        // per MSDN, must manually set the length for TDH_OUTTYPE_IPV6
                        if ((TDH_INTYPE_BINARY == pTraceInfo->EventPropertyInfoArray[property_count].nonStructType.InType) &&
                            (TDH_OUTTYPE_IPV6  == pTraceInfo->EventPropertyInfoArray[property_count].nonStructType.OutType))
                        {
                            property_length = static_cast<USHORT>(sizeof IN6_ADDR);
                        }
                        ULONG pointer_size = (in_pRecord->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? 4 : 8;
                        ULONG formattedPropertySize = 0;
                        USHORT UserDataConsumed = 0;
                        std::vector<WCHAR> formatted_value;
                        ulret = ::TdhFormatProperty(
                            pTraceInfo,
                            reinterpret_cast<PEVENT_MAP_INFO>(pPropertyMap.data()), 
                            pointer_size,
                            pTraceInfo->EventPropertyInfoArray[property_count].nonStructType.InType,
                            pTraceInfo->EventPropertyInfoArray[property_count].nonStructType.OutType,
                            property_length,
                            UserDataLength,
                            UserData,
                            &formattedPropertySize,
                            NULL,
                            &UserDataConsumed
                           );
                        if (ERROR_INSUFFICIENT_BUFFER == ulret) {
                            formatted_value.resize(formattedPropertySize / sizeof(WCHAR));
                            ulret = ::TdhFormatProperty(
                                pTraceInfo,
                                reinterpret_cast<PEVENT_MAP_INFO>(pPropertyMap.data()), 
                                pointer_size,
                                pTraceInfo->EventPropertyInfoArray[property_count].nonStructType.InType,
                                pTraceInfo->EventPropertyInfoArray[property_count].nonStructType.OutType,
                                property_length,
                                UserDataLength,
                                UserData,
                                &formattedPropertySize,
                                formatted_value.data(),
                                &UserDataConsumed
                           );
                        }
                        if (ulret != ERROR_SUCCESS)
                        {
#ifdef NTL_TDHFORMAT_FATALCONDITION
                            AlwaysFatalCondition(
                                ntl::String::format_string(
                                    L"TdhFormatProperty failed with error %u, EVENT_RECORD %p, TRACE_EVENT_INFO %p",
                                    ulret, in_pRecord, pTraceInfo).c_str());
#endif
                            this->vtraceMapping.push_back(std::make_pair(std::vector<WCHAR>(), 0));
                        }
                        else
                        {
                            UserDataLength -= UserDataConsumed;
                            UserData += UserDataConsumed;
                            //
                            // now add the vaue/size pair to the member std::vector storing all properties
                            //
                            this->vtraceMapping.push_back(std::make_pair(std::move(formatted_value), formattedPropertySize));
                        }
                    }
                    else
                    {
                        // store null values
                        this->vtraceMapping.push_back(std::make_pair(std::vector<WCHAR>(), 0));
                    }
                }
            }
        }
    }
        
    bInit = true;
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  Destructor
//  - nothing to release since all dynamic allocations are in smart pointers
//
////////////////////////////////////////////////////////////////////////////////
inline
EtwRecord::~EtwRecord() NOEXCEPT
{
    // NOTHING
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  Copy Constructor
//
//  - Since the raw buffers are smart pointers, can safely copy 
//    the entire object without failure
//
////////////////////////////////////////////////////////////////////////////////
inline
EtwRecord::EtwRecord(const EtwRecord& in_event) NOEXCEPT
: eventHeader(in_event.eventHeader),
    etwBufferContext(in_event.etwBufferContext),
    v_eventHeaderExtendedData(in_event.v_eventHeaderExtendedData),
    v_pEventHeaderData(in_event.v_pEventHeaderData),
    ptraceEventInfo(in_event.ptraceEventInfo),
    cbtraceEventInfo(in_event.cbtraceEventInfo),
    vtraceProperties(in_event.vtraceProperties),
    vtraceMapping(in_event.vtraceMapping),
    bInit(in_event.bInit)
{
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  Copy assignment operator
//
//  - simple copy & swap
//
////////////////////////////////////////////////////////////////////////////////
inline
EtwRecord& EtwRecord::operator=(const EtwRecord& in_event) NOEXCEPT
{
    EtwRecord temp(in_event);
    this->swap(temp);
    return *this;
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  assignment operator taking a new EVENT_RECORD*
//
//  - simple copy & swap, replacing any prior event record
//
////////////////////////////////////////////////////////////////////////////////
inline
EtwRecord& EtwRecord::operator=(_In_ PEVENT_RECORD out_record)
{
    EtwRecord temp(out_record);
    this->swap(temp);
    this->bInit = true; // explicitly flag to true
    return *this;
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  swap()
//
//  - no-fail operation
//
////////////////////////////////////////////////////////////////////////////////
inline
void EtwRecord::swap(EtwRecord& in_event) NOEXCEPT
{
    using std::swap;
    swap(this->v_eventHeaderExtendedData, in_event.v_eventHeaderExtendedData);
    swap(this->v_pEventHeaderData, in_event.v_pEventHeaderData);
    swap(this->ptraceEventInfo, in_event.ptraceEventInfo);
    swap(this->cbtraceEventInfo, in_event.cbtraceEventInfo);
    swap(this->vtraceProperties, in_event.vtraceProperties);
    swap(this->vtraceMapping, in_event.vtraceMapping);
    swap(this->bInit, in_event.bInit);
    //
    // manually swap these structures
    //
    EVENT_HEADER tempHeader;
    memcpy_s(
        &(tempHeader), // this to temp
        sizeof(EVENT_HEADER),
        &(this->eventHeader),
        sizeof(EVENT_HEADER)
       );
    memcpy_s(
        &(this->eventHeader), // in_event to this
        sizeof(EVENT_HEADER),
        &(in_event.eventHeader),
        sizeof(EVENT_HEADER)
       );
    memcpy_s(
        &(in_event.eventHeader), // temp to in_event
        sizeof(EVENT_HEADER),
        &(tempHeader),
        sizeof(EVENT_HEADER)
       );
    
    ETW_BUFFER_CONTEXT tempBuffContext;
    memcpy_s(
        &(tempBuffContext), // this to temp
        sizeof(ETW_BUFFER_CONTEXT),
        &(this->etwBufferContext),
        sizeof(ETW_BUFFER_CONTEXT)
       );
    memcpy_s(
        &(this->etwBufferContext), // in_event to this
        sizeof(ETW_BUFFER_CONTEXT),
        &(in_event.etwBufferContext),
        sizeof(ETW_BUFFER_CONTEXT)
       );
    memcpy_s(
        &(in_event.etwBufferContext), // temp to in_event
        sizeof(ETW_BUFFER_CONTEXT),
        &(tempBuffContext),
        sizeof(ETW_BUFFER_CONTEXT)
       );
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  Non-member swap() function for EtwRecord
//
//  - Implementing the non-member swap to be usable generically
//
////////////////////////////////////////////////////////////////////////////////
inline
void swap(EtwRecord& a, EtwRecord& b) NOEXCEPT
{
    a.swap(b);
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  writeRecord() 
//
//  - simple text dump of all event properties to a std::wstring object
//
////////////////////////////////////////////////////////////////////////////////
inline
void EtwRecord::writeRecord(std::wstring& out_wsString) const
{
    //
    // write to a temp string - but use the caller's buffer
    //
    std::wstring wsData;
    wsData.swap(out_wsString);
    wsData.clear();

    static const unsigned cch_StackBuffer = 100;
    wchar_t arStackBuffer[cch_StackBuffer];
    GUID guidBuf = {0};
    RPC_WSTR pszGuid = NULL;
    RPC_STATUS uuidStatus = 0;
    ULONG ulData = 0;
    std::wstring wsText;
        
    //
    //
    //  Data from EVENT_HEADER properties
    //
    //
    wsData += L"\n\tThread ID ";
    _itow_s(this->getThreadId(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tProcess ID ";
    _itow_s(this->getProcessId(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tTime Stamp ";
    _ui64tow_s(this->getTimeStamp().QuadPart, arStackBuffer, cch_StackBuffer, 16);
    wsData += L"0x";
    wsData += arStackBuffer;
        
    wsData += L"\n\tProvider ID ";
    guidBuf = this->getProviderId();
    uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
    if (uuidStatus != RPC_S_OK)
    {
        throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecord::writeRecord", false);
    }
    wsData += reinterpret_cast<LPWSTR>(pszGuid);
    ::RpcStringFreeW(&pszGuid);
    pszGuid = NULL;

    wsData += L"\n\tActivity ID ";
    guidBuf = this->getActivityId();
    uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
    if (uuidStatus != RPC_S_OK)
    {
        throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecord::writeRecord", false);
    }
    wsData += reinterpret_cast<LPWSTR>(pszGuid);
    ::RpcStringFreeW(&pszGuid);
    pszGuid = NULL;

    if (this->queryKernelTime(&ulData))
    {
        wsData += L"\n\tKernel Time ";
        _itow_s(ulData, arStackBuffer, 16);
        wsData += L"0x";
        wsData += arStackBuffer;
    }

    if (this->queryUserTime(&ulData))
    {
        wsData += L"\n\tUser Time ";
        _itow_s(ulData, arStackBuffer, 16);
        wsData += L"0x";
        wsData += arStackBuffer;
    }

    wsData += L"\n\tProcessor Time: ";
    _ui64tow_s(this->getProcessorTime(), arStackBuffer, cch_StackBuffer, 16);
    wsData += L"0x";
    wsData += arStackBuffer;

    //
    //
    //  Data from EVENT_DESCRIPTOR properties
    //
    //
    wsData += L"\n\tEvent ID ";
    _itow_s(this->getEventId(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tVersion ";
    _itow_s(this->getVersion(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tChannel ";
    _itow_s(this->getChannel(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tLevel ";
    _itow_s(this->getLevel(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tOpcode ";
    _itow_s(this->getOpcode(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tTask ";
    _itow_s(this->getTask(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tKeyword ";
    _ui64tow_s(this->getKeyword(), arStackBuffer, cch_StackBuffer, 16);
    wsData += L"0x";
    wsData += arStackBuffer;
        
    //
    //
    //  Data from ETW_BUFFER_CONTEXT properties
    //
    //
    wsData += L"\n\tProcessor ";
    _itow_s(this->getProcessorNumber(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tAlignment ";
    _itow_s(this->getAlignment(), arStackBuffer, 10);
    wsData += arStackBuffer;

    wsData += L"\n\tLogger ID ";
    _itow_s(this->getLoggerId(), arStackBuffer, 10);
    wsData += arStackBuffer;

    //
    //
    //  Data from EVENT_HEADER_EXTENDED_DATA_ITEM properties
    //
    //
    if (this->queryRelatedActivityId(&guidBuf))
    {
        wsData += L"\n\tRelated Activity ID ";
        uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
        if (uuidStatus != RPC_S_OK)
        {
            throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecord::writeRecord", false);
        }
        wsData += reinterpret_cast<LPWSTR>(pszGuid);
        ::RpcStringFreeW(&pszGuid);
        pszGuid = NULL;
    }
        
    std::vector<BYTE> pSID;
    size_t cbSID = 0;
    if (this->querySID(pSID, &cbSID))
    {
        wsData += L"\n\tSID ";
        LPWSTR szSID = NULL;
        if (::ConvertSidToStringSidW(pSID.data(), &szSID))
        {
            wsData += szSID;
            ::LocalFree(szSID);
        }
        else
        {
            DWORD gle = ::GetLastError();
            throw ntl::Exception(gle, L"ConvertSidToStringSid", L"EtwRecord::writeRecord", false);
        }
    }
        
    if (this->queryTerminalSessionId(&ulData))
    {
        wsData += L"\n\tTerminal Session ID ";
        _itow_s(ulData, arStackBuffer, 10);
        wsData += arStackBuffer;
    }
        
    if (this->queryTransactionInstanceId(&ulData))
    {
        wsData += L"\n\tTransaction Instance ID ";
        _itow_s(ulData, arStackBuffer, 10);
        wsData += arStackBuffer;
    }
        
    if (this->queryTransactionParentInstanceId(&ulData))
    {
        wsData += L"\n\tTransaction Parent Instance ID ";
        _itow_s(ulData, arStackBuffer, 10);
        wsData += arStackBuffer;
    }
        
    if (this->queryTransactionParentGuid(&guidBuf))
    {
        wsData += L"\n\tTransaction Parent GUID ";
        uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
        if (uuidStatus != RPC_S_OK)
        {
            throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecord::writeRecord", false);
        }
        wsData += reinterpret_cast<LPWSTR>(pszGuid);
        ::RpcStringFreeW(&pszGuid);
        pszGuid = NULL;
    }
        
    //
    //
    //  Accessors for TRACE_EVENT_INFO properties
    //
    //
    if (this->queryProviderGuid(&guidBuf))
    {
        wsData += L"\n\tProvider GUID ";
        uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
        if (uuidStatus != RPC_S_OK)
        {
            throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecord::writeRecord", false);
        }
        wsData += reinterpret_cast<LPWSTR>(pszGuid);
        ::RpcStringFreeW(&pszGuid);
        pszGuid = NULL;
    }
        
    DECODING_SOURCE dsource;
    if (this->queryDecodingSource(&dsource))
    {
        wsData += L"\n\tDecoding Source ";
        switch (dsource)
        {
            case DecodingSourceXMLFile:
                wsData += L"DecodingSourceXMLFile";
                break;
            case DecodingSourceWbem:
                wsData += L"DecodingSourceWbem";
                break;
            case DecodingSourceWPP:
                wsData += L"DecodingSourceWPP";
                break;
        }
    }
        
    if (this->queryProviderName(wsText))
    {
        wsData += L"\n\tProvider Name " + wsText;
    }
        
    if (this->queryLevelName(wsText))
    {
        wsData += L"\n\tLevel Name " + wsText;
    }
        
    if (this->queryChannelName(wsText))
    {
        wsData += L"\n\tChannel Name " + wsText;
    }
        
    std::vector<std::wstring> keywordData;
    if (this->queryKeywords(keywordData))
    {
        wsData += L"\n\tKeywords [";
        for (std::vector<std::wstring>::iterator iterKeywords = keywordData.begin(), iterEnd = keywordData.end();
                iterKeywords != iterEnd;
                ++iterKeywords)
        {
            wsData += *iterKeywords;
        }
        wsData += L"]";
    }
        
    if (this->queryTaskName(wsText))
    {
        wsData += L"\n\tTask Name " + wsText;
    }
        
    if (this->queryOpcodeName(wsText))
    {
        wsData += L"\n\tOpcode Name " + wsText;
    }
        
    if (this->queryEventMessage(wsText))
    {
        wsData += L"\n\tEvent Message " + wsText;
    }
        
    if (this->queryProviderMessageName(wsText))
    {
        wsData += L"\n\tProvider Message Name " + wsText;
    }
        
    if (this->queryPropertyCount(&ulData))
    {
        wsData += L"\n\tTotal Property Count ";
        _itow_s(ulData, arStackBuffer, 10);
        wsData += arStackBuffer;
    }

    if (this->queryTopLevelPropertyCount(&ulData))
    {
        wsData += L"\n\tTop Level Property Count ";
        _itow_s(ulData, arStackBuffer, 10);
        wsData += arStackBuffer;
            
        if (ulData > 0)
        {
            const BYTE* pByteInfo = this->ptraceEventInfo.data();
            const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
            wsData += L"\n\tProperty Names:";
            for (unsigned long ulCount = 0; ulCount < ulData; ++ulCount)
            {
                wsData.append(L"\n\t\t");
                wsData.append(reinterpret_cast<const wchar_t*>(pByteInfo + pTraceInfo->EventPropertyInfoArray[ulCount].NameOffset));
                wsData.append(L": ");
                wsData.append(this->buildEventPropertyString(ulCount));
            }
        }
    }
        
    //
    // swap and return
    //
    out_wsString.swap(wsData);
}
inline
void EtwRecord::writeFormattedMessage(std::wstring& wsData, bool withDetails) const
{
    ULONG ulData;
    if (this->queryTopLevelPropertyCount(&ulData) && (ulData > 0))
    {
        const BYTE* pByteInfo = this->ptraceEventInfo.data();
        const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());

        std::wstring wsProperties;
        std::wstring wsPropertyValue;
        std::vector<std::wstring> wsPropertyVector;
        for (unsigned long ulCount = 0; ulCount < ulData; ++ulCount)
        {
            wsProperties.append(L"\n[");
            wsProperties.append(reinterpret_cast<const wchar_t*>(pByteInfo + pTraceInfo->EventPropertyInfoArray[ulCount].NameOffset));
            wsProperties.append(L"] ");

            // use the mapped string if it's available
            if (this->vtraceMapping[ulCount].first.data() != NULL) 
            {
                wsProperties.append(this->vtraceMapping[ulCount].first.data());
                wsPropertyVector.push_back(this->vtraceMapping[ulCount].first.data());
            }
            else
            {
                wsPropertyValue = this->buildEventPropertyString(ulCount);
                wsProperties.append(wsPropertyValue);
                wsPropertyVector.push_back(wsPropertyValue);
            }
        }
        // need an array of wchar_t* to pass to FormatMessage
        std::vector<LPWSTR> messageArguments;
        for (std::vector<std::wstring>::iterator iter = wsPropertyVector.begin(), iterEnd = wsPropertyVector.end();
                iter != iterEnd;
                ++iter)
        {
            std::wstring& wsProperty(*iter);
            messageArguments.push_back(&wsProperty[0]);
        }

        wsData.assign(L"Event Message: ");
        std::wstring wsEventMessage;
        if (this->queryEventMessage(wsEventMessage)) 
        {
            WCHAR* formattedMessage;
            if (0 != FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                wsEventMessage.c_str(),
                0,
                0,
                reinterpret_cast<LPWSTR>(&formattedMessage), // will be allocated from LocalAlloc
                0,
                reinterpret_cast<va_list*>(&messageArguments[0])))
            {
                // Make sure we free the formattedMessage (even if append throws)
                std::unique_ptr<WCHAR, WCharDeleter> free_message(formattedMessage);
                wsData.append(formattedMessage);
            }
            else
            {
                wsData.append(wsEventMessage);
            }
        }
        if (withDetails)
        {
            wsData.append(L"\nEvent Message Properties:");
            wsData.append(wsProperties);
        }
    } 
    else 
    {
        wsData.clear();
    }
}
////////////////////////////////////////////////////////////////////////////////
//
//  Comparison operators
//
////////////////////////////////////////////////////////////////////////////////
inline
bool EtwRecord::operator==(const EtwRecord& inEvent) const
{
    if (0 != memcmp(&(this->eventHeader), &(inEvent.eventHeader), sizeof(EVENT_HEADER))) return false;
    if (0 != memcmp(&(this->etwBufferContext), &(inEvent.etwBufferContext), sizeof(ETW_BUFFER_CONTEXT))) return false;
    if (this->bInit != inEvent.bInit) return false;
    //
    // a deep comparison of the v_eventHeaderExtendedData member
    //
    // can't just do a byte comparison of the structs since the DataPtr member is a raw ptr value
    // - and can be different raw buffers with the same event
    //
    if (this->v_eventHeaderExtendedData.size() != inEvent.v_eventHeaderExtendedData.size()) return false;
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator thisDataIter = this->v_eventHeaderExtendedData.begin();
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator thisDataEnd  = this->v_eventHeaderExtendedData.end();
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator inEventDataIter = inEvent.v_eventHeaderExtendedData.begin();
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator inEventDataEnd  = inEvent.v_eventHeaderExtendedData.end();
    for ( ; (thisDataIter != thisDataEnd) && (inEventDataIter != inEventDataEnd); ++thisDataIter, ++inEventDataIter)
    {
        if (thisDataIter->ExtType != inEventDataIter->ExtType) return false;
        if (thisDataIter->DataSize != inEventDataIter->DataSize) return false;
        if (0 != memcmp(reinterpret_cast<VOID*>(thisDataIter->DataPtr), reinterpret_cast<VOID*>(inEventDataIter->DataPtr), thisDataIter->DataSize)) return false;
    }
    //
    // a deep comparison of the ptraceEventInfo member
    //
    if (this->cbtraceEventInfo != inEvent.cbtraceEventInfo) return false;
    if (0 != memcmp(this->ptraceEventInfo.data(), inEvent.ptraceEventInfo.data(), this->cbtraceEventInfo)) return false;
    
    return true;
}
inline
bool EtwRecord::operator!=(const EtwRecord& inEvent) const
{
    return !(this->operator==(inEvent));
}
    
////////////////////////////////////////////////////////////////////////////////
//
//  Accessors for EVENT_HEADER properties
//
//  - retrieved from the member variable
//    EVENT_HEADER eventHeader;
//
////////////////////////////////////////////////////////////////////////////////
inline
ULONG EtwRecord::getThreadId() const NOEXCEPT
{
    return eventHeader.ThreadId;
}
inline
ULONG EtwRecord::getProcessId() const NOEXCEPT
{
    return eventHeader.ProcessId;
}
inline
LARGE_INTEGER EtwRecord::getTimeStamp() const NOEXCEPT
{
    return eventHeader.TimeStamp;
}
inline
GUID EtwRecord::getProviderId() const NOEXCEPT
{
    return eventHeader.ProviderId;
}
inline
GUID EtwRecord::getActivityId() const NOEXCEPT
{
    return eventHeader.ActivityId;
}
inline
_Success_(return)
bool EtwRecord::queryKernelTime(_Out_ ULONG * pout_Time) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    if ( (eventHeader.Flags & EVENT_HEADER_FLAG_PRIVATE_SESSION) ||
            (eventHeader.Flags & EVENT_HEADER_FLAG_NO_CPUTIME))
    {
        return false;
    }
    else
    {
        *pout_Time = eventHeader.KernelTime;
        return true;
    }
}
inline
_Success_(return)
bool EtwRecord::queryUserTime(_Out_ ULONG * pout_Time) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    if ( (eventHeader.Flags & EVENT_HEADER_FLAG_PRIVATE_SESSION) ||
            (eventHeader.Flags & EVENT_HEADER_FLAG_NO_CPUTIME))
    {
        return false;
    }
    else
    {
        *pout_Time = eventHeader.UserTime;
        return true;
    }
}
inline
ULONG64 EtwRecord::getProcessorTime() const NOEXCEPT
{
    return eventHeader.ProcessorTime;
}
////////////////////////////////////////////////////////////////////////////////
//
//  Accessors for EVENT_DESCRIPTOR properties
//
//  - retrieved from the member variable
//    EVENT_HEADER eventHeader.EventDescriptor;
//
////////////////////////////////////////////////////////////////////////////////
inline
USHORT EtwRecord::getEventId() const NOEXCEPT
{
    return eventHeader.EventDescriptor.Id;
}
inline
UCHAR EtwRecord::getVersion() const NOEXCEPT
{
    return eventHeader.EventDescriptor.Version;
}
inline
UCHAR EtwRecord::getChannel() const NOEXCEPT
{
    return eventHeader.EventDescriptor.Channel;
}
inline
UCHAR EtwRecord::getLevel() const NOEXCEPT
{
    return eventHeader.EventDescriptor.Level;
}
inline
UCHAR EtwRecord::getOpcode() const NOEXCEPT
{
    return eventHeader.EventDescriptor.Opcode;
}
inline
USHORT EtwRecord::getTask() const NOEXCEPT
{
    return eventHeader.EventDescriptor.Task;
}
inline
ULONGLONG EtwRecord::getKeyword() const NOEXCEPT
{
    return eventHeader.EventDescriptor.Keyword;
}
////////////////////////////////////////////////////////////////////////////////
//
//  Accessors for ETW_BUFFER_CONTEXT properties
//
//  - retrieved from the member variable
//    ETW_BUFFER_CONTEXT etwBufferContext;
//
////////////////////////////////////////////////////////////////////////////////
inline
UCHAR EtwRecord::getProcessorNumber() const NOEXCEPT
{
    return etwBufferContext.ProcessorNumber;
}
inline
UCHAR EtwRecord::getAlignment() const NOEXCEPT
{
    return etwBufferContext.Alignment;
}
inline
USHORT EtwRecord::getLoggerId() const NOEXCEPT
{
    return etwBufferContext.LoggerId;
}
////////////////////////////////////////////////////////////////////////////////
//
//  Accessors for EVENT_HEADER_EXTENDED_DATA_ITEM properties
//
//  - retrieved from the member variable
//    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM> v_eventHeaderExtendedData;
//
//  - required to walk the std::vector to determine if the asked-for property 
//    is in any of the data items stored.
//
////////////////////////////////////////////////////////////////////////////////
inline
_Success_(return)
bool EtwRecord::queryRelatedActivityId(_Out_ GUID * pout_GUID) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    bool bFoundProperty = false;
        
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataIter = v_eventHeaderExtendedData.begin();
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataEnd = v_eventHeaderExtendedData.end();
    for ( ; !bFoundProperty && (dataIter != dataEnd); ++dataIter)
    {
        EVENT_HEADER_EXTENDED_DATA_ITEM tempItem = *dataIter;
            
        if (tempItem.ExtType == EVENT_HEADER_EXT_TYPE_RELATED_ACTIVITYID)
        {
            assert(tempItem.DataSize == sizeof(EVENT_EXTENDED_ITEM_RELATED_ACTIVITYID));
            EVENT_EXTENDED_ITEM_RELATED_ACTIVITYID* relatedID = reinterpret_cast<EVENT_EXTENDED_ITEM_RELATED_ACTIVITYID*>(tempItem.DataPtr);
            *pout_GUID = relatedID->RelatedActivityId;
            bFoundProperty = true;
        }
    }
        
    return bFoundProperty;
}
inline
_Success_(return)
bool EtwRecord::querySID(_Out_ std::vector<BYTE>& out_pSID, _Out_ size_t * pout_cbSize) const
{
    if (!this->bInit) return false;
    
    bool bFoundProperty = false;
        
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataIter = v_eventHeaderExtendedData.begin();
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataEnd = v_eventHeaderExtendedData.end();
    for ( ; !bFoundProperty && (dataIter != dataEnd); ++dataIter)
    {
        EVENT_HEADER_EXTENDED_DATA_ITEM tempItem = *dataIter;
    
        if (tempItem.ExtType == EVENT_HEADER_EXT_TYPE_SID)
        {
            SID* p_temp_SID = reinterpret_cast<SID*>(tempItem.DataPtr);
            out_pSID = std::vector<BYTE>(tempItem.DataSize);
            *pout_cbSize = tempItem.DataSize;
            memcpy_s(
                out_pSID.data(),
                tempItem.DataSize,
                p_temp_SID,
                *pout_cbSize
               );
            bFoundProperty = true;
        }
    }
        
    return bFoundProperty;
}
inline
_Success_(return)
bool EtwRecord::queryTerminalSessionId(_Out_ ULONG * pout_ID) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    bool bFoundProperty = false;
        
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataIter = v_eventHeaderExtendedData.begin();
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataEnd = v_eventHeaderExtendedData.end();
    for ( ; !bFoundProperty && (dataIter != dataEnd); ++dataIter)
    {
        EVENT_HEADER_EXTENDED_DATA_ITEM tempItem = *dataIter;
            
        if (tempItem.ExtType == EVENT_HEADER_EXT_TYPE_TS_ID)
        {
            assert(tempItem.DataSize == sizeof(EVENT_EXTENDED_ITEM_TS_ID));
            EVENT_EXTENDED_ITEM_TS_ID* ts_ID = reinterpret_cast<EVENT_EXTENDED_ITEM_TS_ID*>(tempItem.DataPtr);
            *pout_ID = ts_ID->SessionId;
            bFoundProperty = true;
        }
    }
    
    return bFoundProperty;
}
inline
_Success_(return)
bool EtwRecord::queryTransactionInstanceId(_Out_ ULONG * pout_ID) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    bool bFoundProperty = false;
        
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataIter = v_eventHeaderExtendedData.begin();
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataEnd = v_eventHeaderExtendedData.end();
    for ( ; !bFoundProperty && (dataIter != dataEnd); ++dataIter)
    {
        EVENT_HEADER_EXTENDED_DATA_ITEM tempItem = *dataIter;
            
        if (tempItem.ExtType == EVENT_HEADER_EXT_TYPE_INSTANCE_INFO)
        {
            assert(tempItem.DataSize == sizeof(EVENT_EXTENDED_ITEM_INSTANCE));
            EVENT_EXTENDED_ITEM_INSTANCE* instanceInfo = reinterpret_cast<EVENT_EXTENDED_ITEM_INSTANCE*>(tempItem.DataPtr);
            *pout_ID = instanceInfo->InstanceId;
            bFoundProperty = true;
        }
    }
    
    return bFoundProperty;
}
inline
_Success_(return)
bool EtwRecord::queryTransactionParentInstanceId(_Out_ ULONG * pout_ID) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    bool bFoundProperty = false;
        
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataIter = v_eventHeaderExtendedData.begin();
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataEnd = v_eventHeaderExtendedData.end();
    for ( ; !bFoundProperty && (dataIter != dataEnd); ++dataIter)
    {
        EVENT_HEADER_EXTENDED_DATA_ITEM tempItem = *dataIter;
            
        if (tempItem.ExtType == EVENT_HEADER_EXT_TYPE_INSTANCE_INFO)
        {
            assert(tempItem.DataSize == sizeof(EVENT_EXTENDED_ITEM_INSTANCE));
            EVENT_EXTENDED_ITEM_INSTANCE* instanceInfo = reinterpret_cast<EVENT_EXTENDED_ITEM_INSTANCE*>(tempItem.DataPtr);
            *pout_ID = instanceInfo->ParentInstanceId;
            bFoundProperty = true;
        }
    }
        
    return bFoundProperty;
}
inline
_Success_(return)
bool EtwRecord::queryTransactionParentGuid(_Out_ GUID * pout_GUID) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    bool bFoundProperty = false;
        
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataIter = v_eventHeaderExtendedData.begin();
    std::vector<EVENT_HEADER_EXTENDED_DATA_ITEM>::const_iterator dataEnd = v_eventHeaderExtendedData.end();
    for ( ; !bFoundProperty && (dataIter != dataEnd); ++dataIter)
    {
        EVENT_HEADER_EXTENDED_DATA_ITEM tempItem = *dataIter;
            
        if (tempItem.ExtType == EVENT_HEADER_EXT_TYPE_INSTANCE_INFO)
        {
            assert(tempItem.DataSize == sizeof(EVENT_EXTENDED_ITEM_INSTANCE));
            EVENT_EXTENDED_ITEM_INSTANCE* instanceInfo = reinterpret_cast<EVENT_EXTENDED_ITEM_INSTANCE*>(tempItem.DataPtr);
            *pout_GUID = instanceInfo->ParentGuid;
            bFoundProperty = true;
        }
    }
        
    return bFoundProperty;
}
////////////////////////////////////////////////////////////////////////////////
//
//  Accessors for TRACE_EVENT_INFO properties
//
//  - retrieved from the member variables
//    std::vector<BYTE> ptraceEventInfo;
//    ULONG cbtraceEventInfo;
//
//  - only valid if the EVENT_HEADER_FLAG_STRING_ONLY flag is not set in
//    the parent EVENT_HEADER struct.
//
////////////////////////////////////////////////////////////////////////////////
//
//  options - from the member variable
//     ptraceEventInfo (cbtraceEventInfo == size of that buffer)
//
inline
_Success_(return)
bool EtwRecord::queryProviderGuid(_Out_ GUID * pout_GUID) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    *pout_GUID = pTraceInfo->ProviderGuid;
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryDecodingSource(_Out_ DECODING_SOURCE * pout_SOURCE) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    *pout_SOURCE = pTraceInfo->DecodingSource;
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryProviderName(_Out_ std::wstring& out_wsName) const
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    if (0 == pTraceInfo->ProviderNameOffset)
    {
        return false;
    }

    const wchar_t* szProviderName = reinterpret_cast<const wchar_t*>(this->ptraceEventInfo.data() + pTraceInfo->ProviderNameOffset);
    out_wsName.assign(szProviderName);
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryLevelName(_Out_ std::wstring& out_wsName) const
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    if (0 == pTraceInfo->LevelNameOffset)
    {
        return false;
    }

    const wchar_t* szLevelName = reinterpret_cast<const wchar_t*>(this->ptraceEventInfo.data() + pTraceInfo->LevelNameOffset);
    out_wsName.assign(szLevelName);
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryChannelName(_Out_ std::wstring& out_wsName) const
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    if (0 == pTraceInfo->ChannelNameOffset)
    {
        return false;
    }

    const wchar_t* szChannelName = reinterpret_cast<const wchar_t*>(this->ptraceEventInfo.data() + pTraceInfo->ChannelNameOffset);
    out_wsName.assign(szChannelName);
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryKeywords(_Out_ std::vector<std::wstring> & out_vKeywords) const
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    if (0 == pTraceInfo->KeywordsNameOffset)
    {
        return false;
    }

    const wchar_t* szKeyName = reinterpret_cast<const wchar_t*>(this->ptraceEventInfo.data() + pTraceInfo->KeywordsNameOffset);
    std::vector<std::wstring> vTemp;
    std::wstring wsTemp;
    size_t cchKeySize = 0;
    while (*szKeyName != L'\0')
    {
        cchKeySize = wcslen(szKeyName) + 1;
        wsTemp.assign(szKeyName);
        vTemp.push_back(wsTemp);
        szKeyName += cchKeySize;
    }
    vTemp.swap(out_vKeywords);
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryTaskName(_Out_ std::wstring& out_wsName) const
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    if (0 == pTraceInfo->TaskNameOffset)
    {
        return false;
    }

    const wchar_t* szTaskName = reinterpret_cast<const wchar_t*>(this->ptraceEventInfo.data() + pTraceInfo->TaskNameOffset);
    out_wsName.assign(szTaskName);
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryOpcodeName(_Out_ std::wstring& out_wsName) const
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    if (0 == pTraceInfo->OpcodeNameOffset)
    {
        return false;
    }

    const wchar_t* szOpcodeName = reinterpret_cast<const wchar_t*>(this->ptraceEventInfo.data() + pTraceInfo->OpcodeNameOffset);
    out_wsName.assign(szOpcodeName);
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryEventMessage(_Out_ std::wstring& out_wsName) const
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    if (0 == pTraceInfo->EventMessageOffset)
    {
        return false;
    }

    const wchar_t* szEventMessage = reinterpret_cast<const wchar_t*>(this->ptraceEventInfo.data() + pTraceInfo->EventMessageOffset);
    out_wsName.assign(szEventMessage);
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryProviderMessageName(_Out_ std::wstring& out_wsName) const
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    if (0 == pTraceInfo->ProviderMessageOffset)
    {
        return false;
    }

    const wchar_t* szProviderMessageName = reinterpret_cast<const wchar_t*>(this->ptraceEventInfo.data() + pTraceInfo->ProviderMessageOffset);
    out_wsName.assign(szProviderMessageName);
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryPropertyCount(_Out_ ULONG * pout_Properties) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    *pout_Properties = pTraceInfo->PropertyCount;
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryTopLevelPropertyCount(_Out_ ULONG * pout_TopLevelProperties) const NOEXCEPT
{
    if (!this->bInit) return false;
    
    if ( (this->eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY) || this->ptraceEventInfo.empty())
    {
        return false;
    }
        
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    *pout_TopLevelProperties = pTraceInfo->TopLevelPropertyCount;
    return true;
}
inline
_Success_(return)
bool EtwRecord::queryEventPropertyStringValue(_Out_ std::wstring& out_wsUserEveString) const
{
    if (!this->bInit) return false;
    
    if (eventHeader.Flags & EVENT_HEADER_FLAG_STRING_ONLY)
    {
        // per the flags, the byte array is a null-terminated string
        out_wsUserEveString.assign(reinterpret_cast<const wchar_t*>(this->ptraceEventInfo.data()));
        return true;
    }
    else
    {
        return false;
    }
}

inline 
_Success_(return)
bool EtwRecord::queryEventPropertyName(const unsigned long ulIndex, _Out_ std::wstring& out_wsPropertyName) const
{
    // immediately fail if no top level property count value or the value is 0
    unsigned long ulData = 0;
    if ( !this->queryTopLevelPropertyCount(&ulData) || (0 == ulData))
    {
        out_wsPropertyName.clear();
        return false;
    }
    if(ulIndex >= ulData)
    {
        out_wsPropertyName.clear();
        return false;
    }    

    const BYTE* pByteInfo = this->ptraceEventInfo.data();
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(pByteInfo);
    const wchar_t* szPropertyFound = reinterpret_cast<const wchar_t*>(pByteInfo + pTraceInfo->EventPropertyInfoArray[ulIndex].NameOffset);
    out_wsPropertyName.assign(szPropertyFound);

    return true;
}

inline
_Success_(return)
bool EtwRecord::queryEventProperty(_In_z_ const wchar_t* szPropertyName, _Out_ std::wstring& out_wsPropertyValue) const
{
    // immediately fail if no top level property count value or the value is 0
    unsigned long ulData = 0;
        
    if ( !this->queryTopLevelPropertyCount(&ulData) || (0 == ulData))
    {
        out_wsPropertyValue.clear();
        return false;
    }
    //
    // iterate through each property name looking for a match
    const BYTE* pByteInfo = this->ptraceEventInfo.data();
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    for (unsigned long ulCount = 0; ulCount < ulData; ++ulCount)
    {
        const wchar_t* szPropertyFound = reinterpret_cast<const wchar_t*>(pByteInfo + pTraceInfo->EventPropertyInfoArray[ulCount].NameOffset);
        if (0 == _wcsicmp(szPropertyName, szPropertyFound))
        {
            out_wsPropertyValue.assign( this->buildEventPropertyString(ulCount));
            return true;
        }
    }
    out_wsPropertyValue.clear();
    return false;
}
inline
_Success_(return)
bool  EtwRecord::queryEventProperty(const unsigned long ulIndex, _Out_ std::wstring& out_wsPropertyValue) const
{
    // immediately fail if no top level property count value or the value is 0 or ulIndex is larger than 
    // total number of properties
    unsigned long ulData = 0;
                
    if ( !this->queryTopLevelPropertyCount(&ulData) || (0 == ulData) || (0 == ulIndex) || (ulIndex > ulData))
    {
        out_wsPropertyValue.clear();
        return false;
    }
    //
    // get the property value
    const BYTE* pByteInfo = this->ptraceEventInfo.data();
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    bool bFoundMatch = (NULL != reinterpret_cast<const wchar_t*>(pByteInfo + pTraceInfo->EventPropertyInfoArray[ulIndex-1].NameOffset));      
    if (bFoundMatch) 
    {
        out_wsPropertyValue.assign( this->buildEventPropertyString(ulIndex-1));
    }
    else
    {
        out_wsPropertyValue.clear();
    }               
    return bFoundMatch;                
}
inline
_Success_(return)
bool  EtwRecord::queryEventProperty(_In_z_ const wchar_t* szPropertyName, _Out_ PropertyPair& out_eventPair) const
{
    //
    // immediately fail if no top level property count value or the value is 0
    unsigned long ulData = 0;
    if ( !this->queryTopLevelPropertyCount(&ulData) ||
            (0 == ulData))
    {
        return false;
    }
    //
    // iterate through each property name looking for a match
    bool bFoundMatch = false;
    const BYTE* pByteInfo = this->ptraceEventInfo.data();
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    for (unsigned long ulCount = 0; !bFoundMatch && (ulCount < ulData); ++ulCount)
    {
        const wchar_t* szPropertyFound = reinterpret_cast<const wchar_t*>(pByteInfo + pTraceInfo->EventPropertyInfoArray[ulCount].NameOffset);
        if (0 == _wcsicmp(szPropertyName, szPropertyFound))
        {
            assert(ulCount < this->vtraceProperties.size());
            if (ulCount < this->vtraceProperties.size())
            {
                out_eventPair = this->vtraceProperties[ulCount];
                bFoundMatch = true;
            }
            else
            {
                //
                // something is messed up - the properties found didn't match the # of property values
                // break and exit now
                break;
            }
        }
    }
    return bFoundMatch;
}
inline
std::wstring EtwRecord::buildEventPropertyString(ULONG ulProperty) const
{
    //
    // immediately fail if no top level property count value or the value asked for is out of range
    unsigned long ulData = 0;
    if ( !this->queryTopLevelPropertyCount(&ulData) || (ulProperty >= ulData))
    {
        throw std::runtime_error("EtwRecord - ETW Property value requested is out of range");
    }

    static const unsigned cch_StackBuffer = 100;
    wchar_t arStackBuffer[cch_StackBuffer] = {0};

    std::wstring wsData;
    // BYTE* pByteInfo = this->ptraceEventInfo.get();
    const TRACE_EVENT_INFO* pTraceInfo = reinterpret_cast<const TRACE_EVENT_INFO*>(this->ptraceEventInfo.data());
    // retrive the raw property information
    USHORT propertyOutType = pTraceInfo->EventPropertyInfoArray[ulProperty].nonStructType.OutType;
    ULONG  propertySize = this->vtraceProperties[ulProperty].second;
    const BYTE*  propertyBuf = this->vtraceProperties[ulProperty].first.data();
    // build a string only if the property data > 0 bytes
    if (propertySize > 0)
    {
        //
        // build the string based on the IN and OUT types
        switch (pTraceInfo->EventPropertyInfoArray[ulProperty].nonStructType.InType)
        {
            case TDH_INTYPE_NULL:
            {
                wsData = L"null";
                break;
            }
                
            case TDH_INTYPE_UNICODESTRING:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_STRING;
                }
                // xs:string
                assert(propertyOutType == TDH_OUTTYPE_STRING);
                // - not guaranteed to be NULL terminated
                const wchar_t* wszBuffer = reinterpret_cast<const wchar_t*>(propertyBuf);
                const wchar_t* wszBufferEnd = wszBuffer + (propertySize / 2);
                // don't assign over the final NULL terminator (will embed the null in the std::wstring)
                while ( (wszBuffer < wszBufferEnd) && (L'\0' == *(wszBufferEnd - 1)))
                {
                    --wszBufferEnd;
                }
                wsData.assign(wszBuffer, wszBufferEnd);
                break;
            }
                
            case TDH_INTYPE_ANSISTRING:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_STRING;
                }
                // xs:string
                assert(propertyOutType == TDH_OUTTYPE_STRING);
                // - not guaranteed to be NULL terminated
                const char* szBuffer = reinterpret_cast<const char*>(propertyBuf);
                const char* szBufferEnd = szBuffer + propertySize;
                // don't assign over the final NULL terminator (will embed the null in the std::wstring)
                while ( (szBuffer < szBufferEnd) && (L'\0' == *(szBufferEnd - 1)))
                {
                    --szBufferEnd;
                }
                std::string sData(szBuffer, szBufferEnd);
                // convert to wide
                int iResult = ::MultiByteToWideChar(CP_ACP, 0, sData.c_str(), -1, NULL, 0);
                if (iResult != 0)
                {
                    std::vector<wchar_t> vszprop(iResult, L'\0');
                    iResult = ::MultiByteToWideChar(CP_ACP, 0, sData.c_str(), -1, &vszprop[0], iResult);
                    if (iResult != 0)
                    {
                        wsData = &vszprop[0];
                    }
                }
                break;
            }
                
            case TDH_INTYPE_INT8:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_BYTE;
                }
                // xs:byte
                assert(1 == propertySize);
                char cprop = *(reinterpret_cast<const char*>(propertyBuf));
                assert(propertyOutType == TDH_OUTTYPE_BYTE);
                _itow_s(cprop, arStackBuffer, 10);
                wsData = arStackBuffer;
                break;
            }
                
            case TDH_INTYPE_UINT8:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_UNSIGNEDBYTE;
                }
                // xs:unsignedByte; win:hexInt8
                assert(1 == propertySize);
                unsigned char ucprop = *(reinterpret_cast<const unsigned char*>(propertyBuf));
                if (TDH_OUTTYPE_UNSIGNEDBYTE == propertyOutType)
                {
                    _itow_s(ucprop, arStackBuffer, 10);
                    wsData = arStackBuffer;
                }
                else if (TDH_OUTTYPE_HEXINT8 == propertyOutType)
                {
                    _itow_s(ucprop, arStackBuffer, 16);
                    wsData = L"0x";
                    wsData += arStackBuffer;
                }
                else
                {
                    assert(!"Unknown OUT type for TDH_INTYPE_UINT8" && propertyOutType);
                }
                break;
            }
                
            case TDH_INTYPE_INT16:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_SHORT;
                }
                // xs:short
                assert(2 == propertySize);
                short sprop = *(reinterpret_cast<const short*>(propertyBuf));
                assert(propertyOutType == TDH_OUTTYPE_SHORT);
                _itow_s(sprop, arStackBuffer, 10);
                wsData = arStackBuffer;
                break;
            }
                
            case TDH_INTYPE_UINT16:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_UNSIGNEDSHORT;
                }
                // xs:unsignedShort; win:Port; win:HexInt16
                assert(2 == propertySize);
                unsigned short usprop = *(reinterpret_cast<const unsigned short*>(propertyBuf));
                if (TDH_OUTTYPE_UNSIGNEDSHORT == propertyOutType)
                {
                    _itow_s(usprop, arStackBuffer, 10);
                    wsData = arStackBuffer;
                }
                else if (TDH_OUTTYPE_PORT == propertyOutType)
                {
                    _itow_s(::ntohs(usprop), arStackBuffer, 10);
                    wsData = arStackBuffer;
                }
                else if (TDH_OUTTYPE_HEXINT16 == propertyOutType)
                {
                    _itow_s(usprop, arStackBuffer, 16);
                    wsData = L"0x";
                    wsData += arStackBuffer;
                }
                else
                {
                    assert(!"Unknown OUT type for TDH_INTYPE_UINT16" && propertyOutType);
                }
                break;
            }

            case TDH_INTYPE_INT32:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_INT;
                }
                // xs:int
                assert(4 == propertySize);
                int iprop = *(reinterpret_cast<const int*>(propertyBuf));
                assert(propertyOutType == TDH_OUTTYPE_INT);
                _itow_s(iprop, arStackBuffer, 10);
                wsData = arStackBuffer;
                break;
            }
                
            case TDH_INTYPE_UINT32:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_UNSIGNEDINT;
                }
                // xs:unsignedInt, win:PID, win:TID, win:IPv4, win:ETWTIME, win:ErrorCode, win:HexInt32
                assert(4 == propertySize);
                unsigned int uiprop = *(reinterpret_cast<const unsigned int*>(propertyBuf));
                if ( (TDH_OUTTYPE_UNSIGNEDINT == propertyOutType) ||
                     (TDH_OUTTYPE_UNSIGNEDLONG == propertyOutType) ||
                     (TDH_OUTTYPE_PID == propertyOutType) ||
                     (TDH_OUTTYPE_TID == propertyOutType) ||
                     (TDH_OUTTYPE_ETWTIME == propertyOutType))
                {
                    // display as an unsigned int
                    _itow_s(uiprop, arStackBuffer, 10);
                    wsData = arStackBuffer;
                }
                else if (TDH_OUTTYPE_IPV4 == propertyOutType)
                {
                    // display as a v4 address
                    ::RtlIpv4AddressToStringW(
                        reinterpret_cast<const IN_ADDR*>(propertyBuf),
                        arStackBuffer
                       );
                    wsData += arStackBuffer;
                }
                else if ( (TDH_OUTTYPE_HEXINT32 == propertyOutType) ||
                          (TDH_OUTTYPE_ERRORCODE == propertyOutType) ||
                          (TDH_OUTTYPE_WIN32ERROR == propertyOutType) ||
                          (TDH_OUTTYPE_NTSTATUS == propertyOutType) ||
                          (TDH_OUTTYPE_HRESULT == propertyOutType))
                {
                    // display as a hex value
                    _itow_s(uiprop, arStackBuffer, 16);
                    wsData = L"0x";
                    wsData += arStackBuffer;
                }
                else
                {
                    AlwaysFatalCondition(
                        L"Unknown TDH_OUTTYPE [%u] for the TDH_INTYPE_UINT32 value [%u]",
                        propertyOutType, 
                        uiprop);
                }
                break;
            }
                
            case TDH_INTYPE_INT64:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_LONG;
                }
                // xs:long
                assert(8 == propertySize);
                __int64 i64prop = *(reinterpret_cast<const __int64*>(propertyBuf));
                assert(propertyOutType == TDH_OUTTYPE_LONG);
                _i64tow_s(i64prop, arStackBuffer, cch_StackBuffer, 10);
                wsData = arStackBuffer;
                break;
            }
                
            case TDH_INTYPE_UINT64:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_UNSIGNEDLONG;
                }
                // xs:unsignedLong, win:HexInt64
                assert(8 == propertySize);
                unsigned __int64 ui64prop = *(reinterpret_cast<const unsigned __int64*>(propertyBuf));
                if (TDH_OUTTYPE_UNSIGNEDLONG == propertyOutType)
                {
                    _ui64tow_s(ui64prop, arStackBuffer, cch_StackBuffer, 10);
                    wsData = arStackBuffer;
                }
                else if (TDH_OUTTYPE_HEXINT64 == propertyOutType)
                {
                    _ui64tow_s(ui64prop, arStackBuffer, cch_StackBuffer, 16);
                    wsData = L"0x";
                    wsData += arStackBuffer;
                }
                else
                {
                    assert(!"Unknown OUT type for TDH_INTYPE_UINT64" && propertyOutType);
                }
                break;
            }
                
            case TDH_INTYPE_FLOAT:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_FLOAT;
                }
                // xs:float 
                float fprop = *(reinterpret_cast<const float*>(propertyBuf));
                assert(propertyOutType == TDH_OUTTYPE_FLOAT);
                swprintf_s(
                    arStackBuffer,
                    cch_StackBuffer,
                    L"%f",
                    fprop
                   );
                wsData += arStackBuffer;
                break;
            }
                
            case TDH_INTYPE_DOUBLE:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_DOUBLE;
                }
                // xs:double
                double dbprop = *(reinterpret_cast<const double*>(propertyBuf));
                assert(propertyOutType == TDH_OUTTYPE_DOUBLE);
                swprintf_s(
                    arStackBuffer,
                    cch_StackBuffer,
                    L"%f",
                    dbprop
                   );
                wsData += arStackBuffer;
                break;
            }
                
            case TDH_INTYPE_BOOLEAN:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_BOOLEAN;
                }
                // xs:boolean
                assert(propertyOutType == TDH_OUTTYPE_BOOLEAN);
                int iprop = *(reinterpret_cast<const int*>(propertyBuf));
                if (0 == iprop)
                {
                    wsData = L"false";
                }
                else
                {
                    wsData = L"true";
                }
                break;
            }
                
            case TDH_INTYPE_BINARY:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_HEXBINARY;
                }
                // xs:hexBinary, win:IPv6 (16 bytes), win:SocketAddress
                if (TDH_OUTTYPE_HEXBINARY == propertyOutType)
                {
                    wsData = L'[';
                    const BYTE* pbuffer = propertyBuf;
                    for (unsigned long ulBits = 0; ulBits < propertySize; ++ulBits)
                    {
                        unsigned char chData = pbuffer[ulBits];
                        _itow_s(chData, arStackBuffer, 16);
                        wsData += arStackBuffer;
                    }
                    wsData += L']';
                }
                else if (TDH_OUTTYPE_IPV6 == propertyOutType)
                {
                    ::RtlIpv6AddressToStringW(
                        reinterpret_cast<const IN6_ADDR*>(propertyBuf),
                        arStackBuffer
                       );
                    wsData += arStackBuffer;
                }
                else if (TDH_OUTTYPE_SOCKETADDRESS == propertyOutType)
                {
                    DWORD dwSize = cch_StackBuffer;
                    const sockaddr* socketAddress = reinterpret_cast<const sockaddr*>(propertyBuf);
                    sockaddr addressValue = *socketAddress;
                    int iReturn = ::WSAAddressToStringW(
                        &addressValue,
                        propertySize,
                        NULL,
                        arStackBuffer,
                        &dwSize
                       );
                    if (0 == iReturn)
                    {
                        wsData = arStackBuffer;
                    }
                }
                else
                {
                    assert(!"Unknown OUT type for TDH_INTYPE_BINARY" && propertyOutType);
                }
                break;
            }
                
            case TDH_INTYPE_GUID:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_GUID;
                }
                // xs:GUID 
                assert(TDH_OUTTYPE_GUID  == propertyOutType);
                assert(sizeof(GUID) == propertySize);
                if (sizeof(GUID) == propertySize)
                {
                    RPC_WSTR pszGuid = NULL;
                    RPC_STATUS uuidStatus = ::UuidToStringW(
                        reinterpret_cast<const GUID*>(propertyBuf),
                        &pszGuid
                       );
                    if (RPC_S_OK == uuidStatus)
                    {
                        wsData = reinterpret_cast<LPWSTR>(pszGuid);
                        ::RpcStringFreeW(&pszGuid);
                    }
                }
                break;
            }
                
            case TDH_INTYPE_POINTER:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_HEXINT64;
                }
                // win:hexInt64 
                if (4 == propertySize)
                {
                    assert(TDH_OUTTYPE_HEXINT64 == propertyOutType);
                    unsigned long usprop = *(reinterpret_cast<const unsigned long*>(propertyBuf));
                    _itow_s(usprop, arStackBuffer, 16);
                    wsData = L"0x";
                    wsData += arStackBuffer;
                }
                else if (8 == propertySize)
                {
                    assert(TDH_OUTTYPE_HEXINT64 == propertyOutType);
                    unsigned __int64 ui64prop = *(reinterpret_cast<const unsigned __int64*>(propertyBuf));
                    _ui64tow_s(ui64prop, arStackBuffer, cch_StackBuffer, 16);
                    wsData = L"0x";
                    wsData += arStackBuffer;
                }
                else
                {
                    wprintf(L"TDH_INTYPE_POINTER was called with a %d -size value\n", propertySize);
                }
                break;
            }
                
            case TDH_INTYPE_FILETIME:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_DATETIME;
                }
                // xs:dateTime
                assert(sizeof(FILETIME) == propertySize);
                if (sizeof(FILETIME) == propertySize)
                {
                    FILETIME ft = *(reinterpret_cast<const FILETIME*>(propertyBuf));
                    LARGE_INTEGER li;
                    li.LowPart = ft.dwLowDateTime;
                    li.HighPart = ft.dwHighDateTime;
                    _ui64tow_s(li.QuadPart, arStackBuffer, cch_StackBuffer, 16);
                    wsData = L"0x";
                    wsData += arStackBuffer;
                }
                break;
            }
                
            case TDH_INTYPE_SYSTEMTIME:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_DATETIME;
                }
                assert(sizeof(SYSTEMTIME) == propertySize);
                if (sizeof(SYSTEMTIME) == propertySize)
                {
                    SYSTEMTIME st = *(reinterpret_cast<const SYSTEMTIME*>(propertyBuf));
                    _snwprintf_s(
                        arStackBuffer,
                        cch_StackBuffer,
                        99,
                        L"%d/%d/%d - %d:%d:%d::%d",
                        st.wYear, st.wMonth, st.wDay,
                        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds
                       );
                    wsData = arStackBuffer;
                }
                break;
            }
                
            case TDH_INTYPE_SID:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_STRING;
                }
                //
                // first write out the raw binary
                wsData = L'[';
                const BYTE* pbuffer = propertyBuf;
                for (unsigned long ulBits = 0; ulBits < propertySize; ++ulBits)
                {
                    char chData = pbuffer[ulBits];
                    _itow_s(chData, arStackBuffer, 16);
                    wsData += arStackBuffer;
                }
                wsData += L']';
                //
                // now convert if we can to the friendly name
                wchar_t sztemp[1];
                const SID* pSid = reinterpret_cast<const SID*>(pbuffer);
                PSID sidValue = const_cast<SID*>(pSid); // LookupAccountSid is not const-correct
                std::vector<wchar_t> szName;
                std::vector<wchar_t> szDomain;
                DWORD cchName = 0;
                DWORD cchDomain = 0;
                SID_NAME_USE sidNameUse;
                if (!::LookupAccountSid(
                    NULL,
                    sidValue,
                    sztemp,
                    &cchName,
                    sztemp,
                    &cchDomain,
                    &sidNameUse))
                {
                    if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                    {
                        szName.resize(cchName);
                        szDomain.resize(cchDomain);
                        if (::LookupAccountSid(
                            NULL,
                            sidValue,
                            szName.data(),
                            &cchName,
                            szDomain.data(),
                            &cchDomain,
                            &sidNameUse))
                        {
                            wsData += L"  ";
                            wsData += szDomain.data();
                            wsData += L"\\";
                            wsData += szName.data();
                        }
                    }
                }
                break;
            }
                
            case TDH_INTYPE_HEXINT32:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_HEXINT32;
                }
                if (4 == propertySize)
                {
                    assert(TDH_OUTTYPE_HEXINT32 == propertyOutType);
                    unsigned short usprop = *(reinterpret_cast<const unsigned short*>(propertyBuf));
                    _itow_s(usprop, arStackBuffer, 10);
                    wsData = arStackBuffer;
                }
                break;
            }

            case TDH_INTYPE_HEXINT64:
            {
                if (propertyOutType == TDH_OUTTYPE_NULL) {
                    propertyOutType = TDH_OUTTYPE_HEXINT64;
                }
                if (8 == propertySize)
                {
                    assert(TDH_OUTTYPE_HEXINT64 == propertyOutType);
                    unsigned __int64 ui64prop = *(reinterpret_cast<const unsigned __int64*>(propertyBuf));
                    _ui64tow_s(ui64prop, arStackBuffer, cch_StackBuffer, 16);
                    wsData = L"0x";
                    wsData += arStackBuffer;
                }
                break;
            }
        } // switch statement
    }
    return wsData;
}

} // namespace ntl