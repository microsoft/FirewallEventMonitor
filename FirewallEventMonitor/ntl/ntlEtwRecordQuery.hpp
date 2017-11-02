// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// CPP Headers
#include <wchar.h>
#include <vector>
#include <algorithm>
#include <utility>
#include <string>
// OS Headers
#include <Windows.h>
#include <rpc.h>
#include <winsock2.h>
// these 3 headers needed for evntrace.h
#include <wmistr.h>
#include <winmeta.h>
#include <evntcons.h>
#include <evntrace.h>
#include <guiddef.h>
#include <Tdh.h>

// Local headers
#include "ntlException.hpp"
#include "ntlEtwRecord.hpp"

namespace ntl
{

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
//  class EtwRecordQuery
//
//  A EtwRecordQuery object provides a mechanism to make equality 
//      comparisons with EtwRecord objects (objects which encapsulate
//      EVENT_RECORD structures).
//  This class allows the user to only compare properties that they are 
//      interested in, ignoring differences in any other properties.
//  This is important as there are many properties accessible from 
//      EVENT_RECORD structures, and many are optional.
//
//
//  bool Compare(const EtwRecord&) 
//      - the method that explicitly compares the properties set in the
//        EtwRecordQuery object with the passed-by-const-ref EtwRecord.
//
//  void match*( ...)
//      - methods that set specified EVENT_RECORD properties to use when later
//        calling Compare() on an EVENT_RECORD (in a EtwRecord).
//
//
//  All methods that do not have a NOEXCEPT exception specification can throw
//      std::bad_alloc - which is derived from std::exception.
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
    
class EtwRecordQuery
{
public:
    /////////////////////////////////////////////////////////////
    //
    // Default class methods:
    //
    // Constructor
    // Destructor
    // Copy Constructor
    // Copy Assignment operator
    //
    /////////////////////////////////////////////////////////////
    EtwRecordQuery();
    ~EtwRecordQuery() NOEXCEPT;
    EtwRecordQuery(const EtwRecordQuery &);
    EtwRecordQuery& operator=(const EtwRecordQuery &);
    /////////////////////////////////////////////////////////////
    //
    // comparison operators
    //
    /////////////////////////////////////////////////////////////
    bool operator==(const EtwRecordQuery &) const NOEXCEPT;
    bool operator!=(const EtwRecordQuery &) const NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // Implementing swap() to be a friendly container
    //
    /////////////////////////////////////////////////////////////
    void swap(EtwRecordQuery &) NOEXCEPT;
    void writeQuery(std::wstring&) const;
    /////////////////////////////////////////////////////////////
    //
    // Compare of the specified match properties with the 
    //    specified EtwRecord object (passed by const ref).
    //
    // Returns a bool value of that comparison.
    //
    /////////////////////////////////////////////////////////////
    bool Compare(const ntl::EtwRecord &) const NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // EVENT_HEADER fields to be used when comparing with
    //    EtwRecord objects (8)
    //
    /////////////////////////////////////////////////////////////
    void matchThreadId(const ULONG &) NOEXCEPT;
    void matchProcessId(const ULONG &) NOEXCEPT;
    void matchTimeStamp(const LARGE_INTEGER &) NOEXCEPT;
    void matchProviderId(const GUID &) NOEXCEPT;
    void matchActivityId(const GUID &) NOEXCEPT;
    void matchKernelTime(const ULONG &) NOEXCEPT;
    void matchUserTime(const ULONG &) NOEXCEPT;
    void matchProcessorTime(const ULONG64 &) NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // EVENT_DESCRIPTOR fields to be used when comparing with
    //    EtwRecord objects. (7)
    //
    /////////////////////////////////////////////////////////////
    void matchEventId(const USHORT &) NOEXCEPT;
    void matchVersion(const UCHAR &) NOEXCEPT;
    void matchChannel(const UCHAR &) NOEXCEPT;
    void matchLevel(const UCHAR &) NOEXCEPT;
    void matchOpcode(const UCHAR &) NOEXCEPT;
    void matchTask(const USHORT &) NOEXCEPT;
    void matchKeyword(const ULONGLONG &) NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // ETW_BUFFER_CONTEXT fields to be used when comparing with
    //    EtwRecord objects. (3)
    //
    /////////////////////////////////////////////////////////////
    void matchProcessorNumber(const UCHAR &) NOEXCEPT;
    void matchAlignment(const UCHAR &) NOEXCEPT;
    void matchLoggerId(const USHORT &) NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // EVENT_HEADER_EXTENDED_DATA_ITEM fields to be used when
    //    comparing with EtwRecord objects. (6)
    //
    /////////////////////////////////////////////////////////////
    void matchRelatedActivityId(const GUID &) NOEXCEPT;
    void matchSID(_In_reads_bytes_(_dataSize) const BYTE * _data, const size_t _dataSize);
    void matchTerminalSessionId(const ULONG &) NOEXCEPT;
    void matchTransactionInstanceId(const ULONG &) NOEXCEPT;
    void matchTransactionParentInstanceId(const ULONG &) NOEXCEPT;
    void matchTransactionParentGuid(const GUID &) NOEXCEPT;
    /////////////////////////////////////////////////////////////
    //
    // TRACE_EVENT_INFO fields to be used when comparing 
    //    with EtwRecord objects. (15)
    //
    /////////////////////////////////////////////////////////////
    void matchProviderGuid(const GUID &) NOEXCEPT;
    void matchDecodingSource(const DECODING_SOURCE &) NOEXCEPT;
    void matchProviderName(_In_z_ const wchar_t *);
    void matchLevelName(_In_z_ const wchar_t *);
    void matchChannelName(_In_z_ const wchar_t *);
    void matchKeywords(const std::vector<std::vector<wchar_t>> &);
    void matchTaskName(_In_z_ const wchar_t *);
    void matchOpcodeName(_In_z_ const wchar_t *);
    void matchEventMessage(_In_z_ const wchar_t *);
    void matchProviderMessageName(_In_z_ const wchar_t *);
    void matchPropertyCount(const ULONG &) NOEXCEPT;
    void matchTopLevelPropertyCount(const ULONG &) NOEXCEPT;
    void matchProperty(_In_z_ const wchar_t*, _In_z_ const wchar_t*);
    // overrides for matchProperty that help convert common structs into strings
    void matchProperty(_In_z_ const wchar_t* _szAddressName, _In_z_ const wchar_t* _szPortName, _In_ SOCKADDR_STORAGE& _addr);
    void matchProperty(_In_z_ const wchar_t* _szPropertyName, const GUID& _guid);
    
private:
    /////////////////////////////////////////////////////////////
    //
    // All fields have a bool + a data member
    //   bool members start with b
    //   data members start with x_
    //
    /////////////////////////////////////////////////////////////
    bool bMatchThreadId;
    ULONG x_MatchThreadId;
        
    bool bMatchProcessId;
    ULONG x_MatchProcessId;
        
    bool bMatchTimeStamp;
    LARGE_INTEGER x_MatchTimeStamp;
        
    bool bMatchProviderId;
    GUID x_MatchProviderId;
        
    bool bMatchActivityId;
    GUID x_MatchActivityId;
        
    bool bMatchKernelTime;
    ULONG x_MatchKernelTime;
        
    bool bMatchUserTime;
    ULONG x_MatchUserTime;
        
    bool bMatchProcessorTime;
    ULONG64 x_MatchProcessorTime;
        
    bool bMatchEventId;
    USHORT x_MatchEventId;
        
    bool bMatchVersion;
    UCHAR x_MatchVersion;
        
    bool bMatchChannel;
    UCHAR x_MatchChannel;
        
    bool bMatchLevel;
    UCHAR x_MatchLevel;
        
    bool bMatchOpcode;
    UCHAR x_MatchOpcode;
        
    bool bMatchTask;
    USHORT x_MatchTask;
        
    bool bMatchKeyword;
    ULONGLONG x_MatchKeyword;
        
    bool bMatchProcessorNumber;
    UCHAR x_MatchProcessorNumber;
        
    bool bMatchAlignment;
    UCHAR x_MatchAlignment;
        
    bool bMatchLoggerId;
    USHORT x_MatchLoggerId;
        
    bool bMatchRelatedActivityId;
    GUID x_MatchRelatedActivityId;
        
    bool bMatchSID;
    std::vector<BYTE> x_MatchSID;
    size_t x_MatchSIDSize;
        
    bool bMatchTerminalSessionId;
    ULONG x_MatchTerminalSessionId;
        
    bool bMatchTransactionInstanceId;
    ULONG x_MatchTransactionInstanceId;
        
    bool bMatchTransactionParentInstanceId;
    ULONG x_MatchTransactionParentInstanceId;
        
    bool bMatchTransactionParentGuid;
    GUID x_MatchTransactionParentGuid;
        
    bool bMatchProviderGuid;
    GUID x_MatchProviderGuid;
        
    bool bMatchDecodingSource;
    DECODING_SOURCE x_MatchDecodingSource;
        
    bool bMatchProviderName;
    std::vector<wchar_t> x_MatchProviderName;
        
    bool bMatchLevelName;
    std::vector<wchar_t> x_MatchLevelName;
        
    bool bMatchChannelName;
    std::vector<wchar_t> x_MatchChannelName;
        
    bool bMatchKeywords;
    std::vector<std::vector<wchar_t>> x_MatchKeywords;
        
    bool bMatchTaskName;
    std::vector<wchar_t> x_MatchTaskName;
            
    bool bMatchOpcodeName;
    std::vector<wchar_t> x_MatchOpcodeName;
            
    bool bMatchEventMessage;
    std::vector<wchar_t> x_MatchEventMessage;
            
    bool bMatchProviderMessageName;
    std::vector<wchar_t> x_MatchProviderMessageName;
            
    bool bMatchPropertyCount;
    ULONG x_MatchPropertyCount;
        
    bool bMatchTopLevelPropertyCount;
    ULONG x_MatchTopLevelPropertyCount;
        
    typedef struct std::pair<std::vector<wchar_t>, std::vector<wchar_t>>  ntlPropertyQueryPair;
    bool bMatchProperty;
    std::vector<ntlPropertyQueryPair> x_MatchProperty;

private:
    static bool ntlPropertyQueryPairPredicate(ntlPropertyQueryPair pair1, ntlPropertyQueryPair pair2)
    {
        bool firstVectorsEqual = std::equal(pair1.first.begin(), pair1.first.end(), pair2.first.begin());
        bool secondVectorsEqual = std::equal(pair1.second.begin(), pair1.second.end(), pair2.second.begin());

        return firstVectorsEqual && secondVectorsEqual;
    }

};
    
    
class EtwRecordQueryPredicate
{
public:
    EtwRecordQueryPredicate(const EtwRecordQuery& _query)
    : query(_query)
    {
    }
    ~EtwRecordQueryPredicate()
    {
    }
        
    bool operator() (const ntl::EtwRecord& event)
    {
        return this->query.Compare(event);
    }
private:
    EtwRecordQuery query;
};
    
    
inline EtwRecordQuery::EtwRecordQuery()
{
    //
    // only need to explicitly initialize the bools
    //   If the bool is set to false, the corresponding data member
    //   won't be accessed
    //
    bMatchThreadId = false;
    bMatchProcessId = false;
    bMatchTimeStamp = false;
    bMatchProviderId = false;
    bMatchActivityId = false;
    bMatchKernelTime = false;
    bMatchUserTime = false;
    bMatchProcessorTime = false;
    bMatchEventId = false;
    bMatchVersion = false;
    bMatchChannel = false;
    bMatchLevel = false;
    bMatchOpcode = false;
    bMatchTask = false;
    bMatchKeyword = false;
    bMatchProcessorNumber = false;
    bMatchAlignment = false;
    bMatchLoggerId = false;
    bMatchRelatedActivityId = false;
    bMatchSID = false;
    bMatchTerminalSessionId = false;
    bMatchTransactionInstanceId = false;
    bMatchTransactionParentInstanceId = false;
    bMatchTransactionParentGuid = false;
    bMatchProviderGuid = false;
    bMatchDecodingSource = false;
    bMatchProviderName = false;
    bMatchLevelName = false;
    bMatchChannelName = false;
    bMatchKeywords = false;
    bMatchTaskName = false;
    bMatchOpcodeName = false;
    bMatchEventMessage = false;
    bMatchProviderMessageName = false;
    bMatchPropertyCount = false;
    bMatchTopLevelPropertyCount = false;
    bMatchProperty = false;
}
    
inline EtwRecordQuery::~EtwRecordQuery() NOEXCEPT
{
    // NOTHING
}
    
inline
EtwRecordQuery::EtwRecordQuery(const EtwRecordQuery& query)
{
    this->bMatchThreadId = query.bMatchThreadId;
    this->x_MatchThreadId = query.x_MatchThreadId;
        
    this->bMatchProcessId = query.bMatchProcessId;
    this->x_MatchProcessId = query.x_MatchProcessId;
        
    this->bMatchTimeStamp = query.bMatchTimeStamp;
    this->x_MatchTimeStamp = query.x_MatchTimeStamp;
        
    this->bMatchProviderId = query.bMatchProviderId;
    this->x_MatchProviderId = query.x_MatchProviderId;
        
    this->bMatchActivityId = query.bMatchActivityId;
    this->x_MatchActivityId = query.x_MatchActivityId;
        
    this->bMatchKernelTime = query.bMatchKernelTime;
    this->x_MatchKernelTime = query.x_MatchKernelTime;
        
    this->bMatchUserTime = query.bMatchUserTime;
    this->x_MatchUserTime = query.x_MatchUserTime;
        
    this->bMatchProcessorTime = query.bMatchProcessorTime;
    this->x_MatchProcessorTime = query.x_MatchProcessorTime;
        
    this->bMatchEventId = query.bMatchEventId;
    this->x_MatchEventId = query.x_MatchEventId;
        
    this->bMatchVersion = query.bMatchVersion;
    this->x_MatchVersion = query.x_MatchVersion;
        
    this->bMatchChannel = query.bMatchChannel;
    this->x_MatchChannel = query.x_MatchChannel;
        
    this->bMatchLevel = query.bMatchLevel;
    this->x_MatchLevel = query.x_MatchLevel;
        
    this->bMatchOpcode = query.bMatchOpcode;
    this->x_MatchOpcode = query.x_MatchOpcode;
        
    this->bMatchTask = query.bMatchTask;
    this->x_MatchTask = query.x_MatchTask;
        
    this->bMatchKeyword = query.bMatchKeyword;
    this->x_MatchKeyword = query.x_MatchKeyword;
        
    this->bMatchProcessorNumber = query.bMatchProcessorNumber;
    this->x_MatchProcessorNumber = query.x_MatchProcessorNumber;
        
    this->bMatchAlignment = query.bMatchAlignment;
    this->x_MatchAlignment = query.x_MatchAlignment;
        
    this->bMatchLoggerId = query.bMatchLoggerId;
    this->x_MatchLoggerId = query.x_MatchLoggerId;
        
    this->bMatchRelatedActivityId = query.bMatchRelatedActivityId;
    this->x_MatchRelatedActivityId = query.x_MatchRelatedActivityId;
        
    this->bMatchSID = query.bMatchSID;
    this->x_MatchSID = query.x_MatchSID;
    this->x_MatchSIDSize = query.x_MatchSIDSize;
        
    this->bMatchTerminalSessionId = query.bMatchTerminalSessionId;
    this->x_MatchTerminalSessionId = query.x_MatchTerminalSessionId;
        
    this->bMatchTransactionInstanceId = query.bMatchTransactionInstanceId;
    this->x_MatchTransactionInstanceId = query.x_MatchTransactionInstanceId;
        
    this->bMatchTransactionParentInstanceId = query.bMatchTransactionParentInstanceId;
    this->x_MatchTransactionParentInstanceId = query.x_MatchTransactionParentInstanceId;
        
    this->bMatchTransactionParentGuid = query.bMatchTransactionParentGuid;
    this->x_MatchTransactionParentGuid = query.x_MatchTransactionParentGuid;
        
    this->bMatchProviderGuid = query.bMatchProviderGuid;
    this->x_MatchProviderGuid = query.x_MatchProviderGuid;
        
    this->bMatchDecodingSource = query.bMatchDecodingSource;
    this->x_MatchDecodingSource = query.x_MatchDecodingSource;
        
    this->bMatchProviderName = query.bMatchProviderName;
    this->x_MatchProviderName = query.x_MatchProviderName;
        
    this->bMatchLevelName = query.bMatchLevelName;
    this->x_MatchLevelName = query.x_MatchLevelName;
        
    this->bMatchChannelName = query.bMatchChannelName;
    this->x_MatchChannelName = query.x_MatchChannelName;
        
    this->bMatchKeywords = query.bMatchKeywords;
    this->x_MatchKeywords.assign(
        query.x_MatchKeywords.begin(), 
        query.x_MatchKeywords.end()
       );
        
    this->bMatchTaskName = query.bMatchTaskName;
    this->x_MatchTaskName = query.x_MatchTaskName;
            
    this->bMatchOpcodeName = query.bMatchOpcodeName;
    this->x_MatchOpcodeName = query.x_MatchOpcodeName;
            
    this->bMatchEventMessage = query.bMatchEventMessage;
    this->x_MatchEventMessage = query.x_MatchEventMessage;
            
    this->bMatchProviderMessageName = query.bMatchProviderMessageName;
    this->x_MatchProviderMessageName = query.x_MatchProviderMessageName;
            
    this->bMatchPropertyCount = query.bMatchPropertyCount;
    this->x_MatchPropertyCount = query.x_MatchPropertyCount;
        
    this->bMatchTopLevelPropertyCount = query.bMatchTopLevelPropertyCount;
    this->x_MatchTopLevelPropertyCount = query.x_MatchTopLevelPropertyCount;
        
    this->bMatchProperty = query.bMatchProperty;
    this->x_MatchProperty.assign(query.x_MatchProperty.begin(), query.x_MatchProperty.end());
}
    
inline
EtwRecordQuery& EtwRecordQuery::operator=(const EtwRecordQuery& query)
{
    EtwRecordQuery temp(query);
    this->swap(temp);
    return *this;
}
    
inline
bool EtwRecordQuery::operator!=(const EtwRecordQuery& query) const NOEXCEPT
{
    return !(this->operator==(query));
}
inline
bool EtwRecordQuery::operator==(const EtwRecordQuery& query) const NOEXCEPT
{
    if (this->bMatchThreadId == query.bMatchThreadId)
    {
        if (this->bMatchThreadId)
        {
            if (this->x_MatchThreadId != query.x_MatchThreadId)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchProcessId == query.bMatchProcessId)
    {
        if (this->bMatchProcessId)
        {
            if (this->x_MatchProcessId != query.x_MatchProcessId)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchTimeStamp == query.bMatchTimeStamp)
    {
        if (this->bMatchTimeStamp)
        {
            if ( memcmp(
                    &(this->x_MatchTimeStamp), 
                    &(query.x_MatchTimeStamp), 
                    sizeof(LARGE_INTEGER)
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchProviderId == query.bMatchProviderId)
    {
        if (this->bMatchProviderId)
        {
            if ( memcmp(
                    &(this->x_MatchProviderId),
                    &(query.x_MatchProviderId),
                    sizeof(GUID)
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchActivityId == query.bMatchActivityId)
    {
        if (this->bMatchActivityId)
        {
            if ( memcmp(
                    &(this->x_MatchActivityId),
                    &(query.x_MatchActivityId),
                    sizeof(GUID)
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchKernelTime == query.bMatchKernelTime)
    {
        if (this->bMatchKernelTime)
        {
            if (this->x_MatchKernelTime != query.x_MatchKernelTime)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchUserTime == query.bMatchUserTime)
    {
        if (this->bMatchUserTime)
        {
            if (this->x_MatchUserTime != query.x_MatchUserTime)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchProcessorTime == query.bMatchProcessorTime)
    {
        if (this->bMatchProcessorTime)
        {
            if (this->x_MatchProcessorTime != query.x_MatchProcessorTime)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchEventId == query.bMatchEventId)
    {
        if (this->bMatchEventId)
        {
            if (this->x_MatchEventId != query.x_MatchEventId)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchVersion == query.bMatchVersion)
    {
        if (this->bMatchVersion)
        {
            if (this->x_MatchVersion != query.x_MatchVersion)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchChannel == query.bMatchChannel)
    {
        if (this->bMatchChannel)
        {
            if (this->x_MatchChannel != query.x_MatchChannel)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchLevel == query.bMatchLevel)
    {
        if (this->bMatchLevel)
        {
            if (this->x_MatchLevel != query.x_MatchLevel)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchOpcode == query.bMatchOpcode)
    {
        if (this->bMatchOpcode)
        {
            if (this->x_MatchOpcode != query.x_MatchOpcode)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchTask == query.bMatchTask)
    {
        if (this->bMatchTask)
        {
            if (this->x_MatchTask != query.x_MatchTask)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchKeyword == query.bMatchKeyword)
    {
        if (this->bMatchKeyword)
        {
            if (this->x_MatchKeyword != query.x_MatchKeyword)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchProcessorNumber == query.bMatchProcessorNumber)
    {
        if (this->bMatchProcessorNumber)
        {
            if (this->x_MatchProcessorNumber != query.x_MatchProcessorNumber)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchAlignment == query.bMatchAlignment)
    {
        if (this->bMatchAlignment)
        {
            if (this->x_MatchAlignment != query.x_MatchAlignment)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchLoggerId == query.bMatchLoggerId)
    {
        if (this->bMatchLoggerId)
        {
            if (this->x_MatchLoggerId != query.x_MatchLoggerId)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchRelatedActivityId == query.bMatchRelatedActivityId)
    {
        if (this->bMatchRelatedActivityId)
        {
            if ( memcmp(
                    &(this->x_MatchRelatedActivityId),
                    &(query.x_MatchRelatedActivityId),
                    sizeof(GUID)
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchSID == query.bMatchSID)
    {
        if (this->bMatchSID)
        {
            if (this->x_MatchSIDSize == query.x_MatchSIDSize)
            {
                if ( memcmp(
                        &(this->x_MatchSID),
                        &(query.x_MatchSID),
                        this->x_MatchSIDSize
                       ) != 0)
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchTerminalSessionId == query.bMatchTerminalSessionId)
    {
        if (this->bMatchTerminalSessionId)
        {
            if (this->x_MatchTerminalSessionId != query.x_MatchTerminalSessionId)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchTransactionInstanceId == query.bMatchTransactionInstanceId)
    {
        if (this->bMatchTransactionInstanceId)
        {
            if (this->x_MatchTransactionInstanceId != query.x_MatchTransactionInstanceId)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchTransactionParentInstanceId == query.bMatchTransactionParentInstanceId)
    {
        if (this->bMatchTransactionParentInstanceId)
        {
            if (this->x_MatchTransactionParentInstanceId != query.x_MatchTransactionParentInstanceId)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchTransactionParentGuid == query.bMatchTransactionParentGuid)
    {
        if (this->bMatchTransactionParentGuid)
        {
            if ( memcmp(
                    &(this->x_MatchTransactionParentGuid),
                    &(query.x_MatchTransactionParentGuid),
                    sizeof(GUID)
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchProviderGuid == query.bMatchProviderGuid)
    {
        if (this->bMatchProviderGuid)
        {
            if ( memcmp(
                    &(this->x_MatchProviderGuid),
                    &(query.x_MatchProviderGuid),
                    sizeof(GUID)
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchDecodingSource == query.bMatchDecodingSource)
    {
        if (this->bMatchDecodingSource)
        {
            if ( memcmp(
                    &(this->x_MatchDecodingSource),
                    &(query.x_MatchDecodingSource),
                    sizeof(DECODING_SOURCE)
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchProviderName == query.bMatchProviderName)
    {
        if (this->bMatchProviderName)
        {
            if ( lstrcmpiW(
                    this->x_MatchProviderName.data(),
                    query.x_MatchProviderName.data()
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchLevelName == query.bMatchLevelName)
    {
        if (this->bMatchLevelName)
        {
            if ( lstrcmpiW(
                    this->x_MatchLevelName.data(),
                    query.x_MatchLevelName.data()
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchChannelName == query.bMatchChannelName)
    {
        if (this->bMatchChannelName)
        {
            if ( lstrcmpiW(
                    this->x_MatchChannelName.data(),
                    query.x_MatchChannelName.data()
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchKeywords == query.bMatchKeywords)
    {
        if (this->bMatchKeywords)
        {
            if (this->x_MatchKeywords.size() == query.x_MatchKeywords.size())
            {
                if ( ! std::equal(
                        this->x_MatchKeywords.begin(), 
                        this->x_MatchKeywords.end(), 
                        query.x_MatchKeywords.begin()))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchTaskName == query.bMatchTaskName)
    {
        if (this->bMatchTaskName)
        {
            if ( lstrcmpiW(
                    this->x_MatchTaskName.data(),
                    query.x_MatchTaskName.data()
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchOpcodeName == query.bMatchOpcodeName)
    {
        if (this->bMatchOpcodeName)
        {
            if ( lstrcmpiW(
                    this->x_MatchOpcodeName.data(),
                    query.x_MatchOpcodeName.data()
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchEventMessage == query.bMatchEventMessage)
    {
        if (this->bMatchEventMessage)
        {
            if ( lstrcmpiW(
                    this->x_MatchEventMessage.data(),
                    query.x_MatchEventMessage.data()
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchProviderMessageName == query.bMatchProviderMessageName)
    {
        if (this->bMatchProviderMessageName)
        {
            if ( lstrcmpiW(
                    this->x_MatchProviderMessageName.data(),
                    query.x_MatchProviderMessageName.data()
                   ) != 0)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchPropertyCount == query.bMatchPropertyCount)
    {
        if (this->bMatchPropertyCount)
        {
            if (this->x_MatchPropertyCount != query.x_MatchPropertyCount)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchTopLevelPropertyCount == query.bMatchTopLevelPropertyCount)
    {
        if (this->bMatchTopLevelPropertyCount)
        {
            if (this->x_MatchTopLevelPropertyCount != query.x_MatchTopLevelPropertyCount)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
        
    if (this->bMatchProperty == query.bMatchProperty)
    {
        if (this->bMatchProperty)
        {
            if (this->x_MatchProperty.size() == query.x_MatchProperty.size())
            {
                if ( ! std::equal(
                        this->x_MatchProperty.begin(), 
                        this->x_MatchProperty.end(), 
                        query.x_MatchProperty.begin(),
                        ntlPropertyQueryPairPredicate))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}
    
inline
void EtwRecordQuery::swap(EtwRecordQuery& query) NOEXCEPT
{
    using std::swap;
        
    swap(this->bMatchThreadId, query.bMatchThreadId);
    swap(this->x_MatchThreadId, query.x_MatchThreadId);
        
    swap(this->bMatchProcessId, query.bMatchProcessId);
    swap(this->x_MatchProcessId, query.x_MatchProcessId);
        
    swap(this->bMatchTimeStamp, query.bMatchTimeStamp);
    // manually swap the struct
    LARGE_INTEGER tempLargeInt;
    ::CopyMemory(
        &(tempLargeInt), // this to temp
        &(this->x_MatchTimeStamp),
        sizeof(LARGE_INTEGER)
       );
    ::CopyMemory(
        &(this->x_MatchTimeStamp), // query to this
        &(query.x_MatchTimeStamp),
        sizeof(LARGE_INTEGER)
       );
    ::CopyMemory(
        &(query.x_MatchTimeStamp), // temp to query
        &(tempLargeInt),
        sizeof(LARGE_INTEGER)
       );
    
    swap(this->bMatchProviderId, query.bMatchProviderId);
    // manually swap the struct
    GUID tempGUID;
    ::CopyMemory(
        &(tempGUID), // this to temp
        &(this->x_MatchProviderId),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(this->x_MatchProviderId), // query to this
        &(query.x_MatchProviderId),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(query.x_MatchProviderId), // temp to query
        &(tempGUID),
        sizeof(GUID)
       );
        
    swap(this->bMatchActivityId, query.bMatchActivityId);
    // manually swap the struct
    ::CopyMemory(
        &(tempGUID), // this to temp
        &(this->x_MatchActivityId),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(this->x_MatchActivityId), // query to this
        &(query.x_MatchActivityId),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(query.x_MatchActivityId), // temp to query
        &(tempGUID),
        sizeof(GUID)
       );
    
    swap(this->bMatchKernelTime, query.bMatchKernelTime);
    swap(this->x_MatchKernelTime, query.x_MatchKernelTime);
        
    swap(this->bMatchUserTime, query.bMatchUserTime);
    swap(this->x_MatchUserTime, query.x_MatchUserTime);
        
    swap(this->bMatchProcessorTime, query.bMatchProcessorTime);
    swap(this->x_MatchProcessorTime, query.x_MatchProcessorTime);
        
    swap(this->bMatchEventId, query.bMatchEventId);
    swap(this->x_MatchEventId, query.x_MatchEventId);
        
    swap(this->bMatchVersion, query.bMatchVersion);
    swap(this->x_MatchVersion, query.x_MatchVersion);
        
    swap(this->bMatchChannel, query.bMatchChannel);
    swap(this->x_MatchChannel, query.x_MatchChannel);
        
    swap(this->bMatchLevel, query.bMatchLevel);
    swap(this->x_MatchLevel, query.x_MatchLevel);
        
    swap(this->bMatchOpcode, query.bMatchOpcode);
    swap(this->x_MatchOpcode, query.x_MatchOpcode);
        
    swap(this->bMatchTask, query.bMatchTask);
    swap(this->x_MatchTask, query.x_MatchTask);
        
    swap(this->bMatchKeyword, query.bMatchKeyword);
    swap(this->x_MatchKeyword, query.x_MatchKeyword);
        
    swap(this->bMatchProcessorNumber, query.bMatchProcessorNumber);
    swap(this->x_MatchProcessorNumber, query.x_MatchProcessorNumber);
        
    swap(this->bMatchAlignment, query.bMatchAlignment);
    swap(this->x_MatchAlignment, query.x_MatchAlignment);
        
    swap(this->bMatchLoggerId, query.bMatchLoggerId);
    swap(this->x_MatchLoggerId, query.x_MatchLoggerId);
        
    swap(this->bMatchRelatedActivityId, query.bMatchRelatedActivityId);
    // manually swap the struct
    ::CopyMemory(
        &(tempGUID), // this to temp
        &(this->x_MatchRelatedActivityId),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(this->x_MatchRelatedActivityId), // match to this
        &(query.x_MatchRelatedActivityId),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(query.x_MatchRelatedActivityId), // temp to match
        &(tempGUID),
        sizeof(GUID)
       );
        
    swap(this->bMatchSID, query.bMatchSID);
    swap(this->x_MatchSID, query.x_MatchSID);
    swap(this->x_MatchSIDSize, query.x_MatchSIDSize);
        
    swap(this->bMatchTerminalSessionId, query.bMatchTerminalSessionId);
    swap(this->x_MatchTerminalSessionId, query.x_MatchTerminalSessionId);
        
    swap(this->bMatchTransactionInstanceId, query.bMatchTransactionInstanceId);
    swap(this->x_MatchTransactionInstanceId, query.x_MatchTransactionInstanceId);
        
    swap(this->bMatchTransactionParentInstanceId, query.bMatchTransactionParentInstanceId);
    swap(this->x_MatchTransactionParentInstanceId, query.x_MatchTransactionParentInstanceId);
        
    swap(this->bMatchTransactionParentGuid, query.bMatchTransactionParentGuid);
    // manually swap the struct
    ::CopyMemory(
        &(tempGUID), // this to temp
        &(this->x_MatchTransactionParentGuid),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(this->x_MatchTransactionParentGuid), // match to this
        &(query.x_MatchTransactionParentGuid),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(query.x_MatchTransactionParentGuid), // temp to match
        &(tempGUID),
        sizeof(GUID)
       );
        
    swap(this->bMatchProviderGuid, query.bMatchProviderGuid);
    // manually swap the struct
    ::CopyMemory(
        &(tempGUID), // this to temp
        &(this->x_MatchProviderGuid),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(this->x_MatchProviderGuid), // match to this
        &(query.x_MatchProviderGuid),
        sizeof(GUID)
       );
    ::CopyMemory(
        &(query.x_MatchProviderGuid), // temp to match
        &(tempGUID),
        sizeof(GUID)
       );
        
    swap(this->bMatchDecodingSource, query.bMatchDecodingSource);
    // manually swap the struct
    DECODING_SOURCE tempDecodingSource;
    ::CopyMemory(
        &(tempDecodingSource), // this to temp
        &(this->x_MatchDecodingSource),
        sizeof(DECODING_SOURCE)
       );
    ::CopyMemory(
        &(this->x_MatchDecodingSource), // match to this
        &(query.x_MatchDecodingSource),
        sizeof(DECODING_SOURCE)
       );
    ::CopyMemory(
        &(query.x_MatchDecodingSource), // temp to match
        &(tempDecodingSource),
        sizeof(DECODING_SOURCE)
       );
        
    swap(this->bMatchProviderName, query.bMatchProviderName);
    swap(this->x_MatchProviderName, query.x_MatchProviderName);
        
    swap(this->bMatchLevelName, query.bMatchLevelName);
    swap(this->x_MatchLevelName, query.x_MatchLevelName);
        
    swap(this->bMatchChannelName, query.bMatchChannelName);
    swap(this->x_MatchChannelName, query.x_MatchChannelName);
        
    swap(this->bMatchKeywords, query.bMatchKeywords);
    swap(this->x_MatchKeywords, query.x_MatchKeywords);
        
    swap(this->bMatchTaskName, query.bMatchTaskName);
    swap(this->x_MatchTaskName, query.x_MatchTaskName);
            
    swap(this->bMatchOpcodeName, query.bMatchOpcodeName);
    swap(this->x_MatchOpcodeName, query.x_MatchOpcodeName);
            
    swap(this->bMatchEventMessage, query.bMatchEventMessage);
    swap(this->x_MatchEventMessage, query.x_MatchEventMessage);
            
    swap(this->bMatchProviderMessageName, query.bMatchProviderMessageName);
    swap(this->x_MatchProviderMessageName, query.x_MatchProviderMessageName);
            
    swap(this->bMatchPropertyCount, query.bMatchPropertyCount);
    swap(this->x_MatchPropertyCount, query.x_MatchPropertyCount);
        
    swap(this->bMatchTopLevelPropertyCount, query.bMatchTopLevelPropertyCount);
    swap(this->x_MatchTopLevelPropertyCount, query.x_MatchTopLevelPropertyCount);
        
    swap(this->bMatchProperty, query.bMatchProperty);
    swap(this->x_MatchProperty, query.x_MatchProperty);
}
//
// Non-member swap to make the class really usable
//
inline
void swap(EtwRecordQuery& a, EtwRecordQuery& b)
{
    a.swap(b);
}
    
inline
void EtwRecordQuery::writeQuery(std::wstring& wsQuery) const
{
    std::wstring wsData;
    wsData.swap(wsQuery);
    wsData.clear();

    static const unsigned cch_szNumberBuf = 65;
    wchar_t szNumberBuf[65];
    GUID guidBuf = {0};
    RPC_WSTR pszGuid = NULL;
    RPC_STATUS uuidStatus = 0;
        
    //
    //
    //  Data from EVENT_HEADER properties
    //
    //
    if (bMatchThreadId)
    {
        wsData += L"\n\tThread ID ";
        _itow_s(x_MatchThreadId, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchProcessId)
    {
        wsData += L"\n\tProcess ID ";
        _itow_s(x_MatchProcessId, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchTimeStamp)
    {
        wsData += L"\n\tTime Stamp ";
        _ui64tow_s(x_MatchTimeStamp.QuadPart, szNumberBuf, cch_szNumberBuf, 16);
        wsData += L"0x";
        wsData += szNumberBuf;
    }
        
    if (bMatchProviderId)
    {
        wsData += L"\n\tProvider ID ";
        guidBuf = x_MatchProviderId;
        uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
        if (uuidStatus != RPC_S_OK)
        {
            throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecordQuery::writeQuery", false);
        }
        wsData += reinterpret_cast<LPWSTR>(pszGuid);
        ::RpcStringFreeW(&pszGuid);
        pszGuid = NULL;
    }
        
    if (bMatchActivityId)
    {
        wsData += L"\n\tActivity ID ";
        guidBuf = x_MatchActivityId;
        uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
        if (uuidStatus != RPC_S_OK)
        {
            throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecordQuery::writeQuery", false);
        }
        wsData += reinterpret_cast<LPWSTR>(pszGuid);
        ::RpcStringFreeW(&pszGuid);
        pszGuid = NULL;
    }
        
    if (bMatchKernelTime)
    {
        wsData += L"\n\tKernel Time ";
        _itow_s(x_MatchKernelTime, szNumberBuf, 16);
        wsData += L"0x";
        wsData += szNumberBuf;
    }
        
    if (bMatchUserTime)
    {
        wsData += L"\n\tUser Time ";
        _itow_s(x_MatchUserTime, szNumberBuf, 16);
        wsData += L"0x";
        wsData += szNumberBuf;
    }
        
    if (bMatchProcessorTime)
    {
        wsData += L"\n\tProcessor Time: ";
        _ui64tow_s(x_MatchProcessorTime, szNumberBuf, cch_szNumberBuf, 16);
        wsData += L"0x";
        wsData += szNumberBuf;
    }
        
    //
    //
    //  Data from EVENT_DESCRIPTOR properties
    //
    //
    if (bMatchEventId)
    {
        wsData += L"\n\tEvent ID ";
        _itow_s(x_MatchEventId, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchVersion)
    {
        wsData += L"\n\tVersion ";
        _itow_s(x_MatchVersion, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchChannel)
    {
        wsData += L"\n\tChannel ";
        _itow_s(x_MatchChannel, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchLevel)
    {
        wsData += L"\n\tLevel ";
        _itow_s(x_MatchLevel, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchOpcode)
    {
        wsData += L"\n\tOpcode ";
        _itow_s(x_MatchOpcode, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchTask)
    {
        wsData += L"\n\tTask ";
        _itow_s(x_MatchTask, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchKeyword)
    {
        wsData += L"\n\tKeyword ";
        _ui64tow_s(x_MatchKeyword, szNumberBuf, cch_szNumberBuf, 16);
        wsData += L"0x";
        wsData += szNumberBuf;
    }
        
    //
    //
    //  Data from ETW_BUFFER_CONTEXT properties
    //
    //
    if (bMatchProcessorNumber)
    {
        wsData += L"\n\tProcessor ";
        _itow_s(x_MatchProcessorNumber, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchAlignment)
    {
        wsData += L"\n\tAlignment ";
        _itow_s(x_MatchAlignment, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchLoggerId)
    {
        wsData += L"\n\tLogger ID ";
        _itow_s(x_MatchLoggerId, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    //
    //
    //  Data from EVENT_HEADER_EXTENDED_DATA_ITEM properties
    //
    //
    if (bMatchRelatedActivityId)
    {
        wsData += L"\n\tRelated Activity ID ";
        guidBuf = x_MatchRelatedActivityId;
        uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
        if (uuidStatus != RPC_S_OK)
        {
            throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecordQuery::writeQuery", false);
        }
        wsData += reinterpret_cast<LPWSTR>(pszGuid);
        ::RpcStringFreeW(&pszGuid);
        pszGuid = NULL;
    }
        
    if (bMatchSID)
    {
        wsData += L"\n\tSID ";
        std::vector<BYTE> pSID = x_MatchSID;
        LPWSTR szSID = NULL;
        if (::ConvertSidToStringSidW(pSID.data(), &szSID))
        {
            wsData += szSID;
            ::LocalFree(szSID);
        }
        else
        {
            DWORD gle = ::GetLastError();
            throw ntl::Exception(gle, L"ConvertSidToStringSid", L"EtwRecordQuery::writeQuery", false);
        }
    }
    
    if (bMatchTerminalSessionId)
    {
        wsData += L"\n\tTerminal Session ID ";
        _itow_s(x_MatchTerminalSessionId, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchTransactionInstanceId)
    {
        wsData += L"\n\tTransaction Instance ID ";
        _itow_s(x_MatchTransactionInstanceId, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchTransactionParentInstanceId)
    {
        wsData += L"\n\tTransaction Parent Instance ID ";
        _itow_s(x_MatchTransactionParentInstanceId, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchTransactionParentGuid)
    {
        wsData += L"\n\tTransaction Parent GUID ";
        guidBuf = x_MatchTransactionParentGuid;
            
        uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
        if (uuidStatus != RPC_S_OK)
        {
            throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecordQuery::writeQuery", false);
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
    if (bMatchProviderGuid)
    {
        wsData += L"\n\tProvider GUID ";
        guidBuf = x_MatchProviderGuid;
        uuidStatus = ::UuidToStringW(&guidBuf, &pszGuid);
        if (uuidStatus != RPC_S_OK)
        {
            throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecordQuery::writeQuery", false);
        }
        wsData += reinterpret_cast<LPWSTR>(pszGuid);
        ::RpcStringFreeW(&pszGuid);
        pszGuid = NULL;
    }
        
    if (bMatchDecodingSource)
    {
        wsData += L"\n\tDecoding Source ";
        switch (x_MatchDecodingSource)
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
        
    if (bMatchProviderName)
    {
        wsData += L"\n\tProvider Name ";
        wsData += x_MatchProviderName.data();
    }
        
    if (bMatchLevelName)
    {
        wsData += L"\n\tLevel Name ";
        wsData += x_MatchLevelName.data();
    }
        
    if (bMatchChannelName)
    {
        wsData += L"\n\tChannel Name ";
        wsData += x_MatchChannelName.data();
    }
        
    if (bMatchKeywords)
    {
        wsData += L"\n\tKeywords [";
        std::vector<std::vector<wchar_t>>::const_iterator iterKeywords = x_MatchKeywords.begin();
        std::vector<std::vector<wchar_t>>::const_iterator iterEnd = x_MatchKeywords.end();
            
        for ( ; iterKeywords != iterEnd; ++iterKeywords)
        {
            wsData += iterKeywords->data();
        }
        wsData += L"]";
    }
        
    if (bMatchTaskName)
    {
        wsData += L"\n\tTask Name ";
        wsData += x_MatchTaskName.data();
    }
            
    if (bMatchOpcodeName)
    {
        wsData += L"\n\tOpcode Name ";
        wsData += x_MatchOpcodeName.data();
    }
            
    if (bMatchEventMessage)
    {
        wsData += L"\n\tEvent Message ";
        wsData += x_MatchEventMessage.data();
    }
            
    if (bMatchProviderMessageName)
    {
        wsData += L"\n\tProvider Message Name ";
        wsData += x_MatchProviderMessageName.data();
    }
            
    if (bMatchPropertyCount)
    {
        wsData += L"\n\tTotal Property Count ";
        _itow_s(x_MatchPropertyCount, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
        
    if (bMatchTopLevelPropertyCount)
    {
        wsData += L"\n\tTop Level Property Count ";
        _itow_s(x_MatchTopLevelPropertyCount, szNumberBuf, 10);
        wsData += szNumberBuf;
    }
    if (bMatchProperty)
    {
        wsData += L"\n\tProperties [";
        std::vector<ntlPropertyQueryPair>::const_iterator iterProperties = x_MatchProperty.begin();
        std::vector<ntlPropertyQueryPair>::const_iterator iterEnd = x_MatchProperty.end();
            
        for ( ; iterProperties != iterEnd; ++iterProperties)
        {
            wsData += L" (";
            wsData += iterProperties->first.data();
            wsData += L" - ";
            wsData += iterProperties->second.data();
            wsData += L")";
        }
        wsData += L" ]";
    }
    //
    // swap and return
    //
    wsQuery.swap(wsData);
}

inline
bool EtwRecordQuery::Compare(const ntl::EtwRecord& record) const NOEXCEPT
{
    if (bMatchThreadId)
    {
        if (x_MatchThreadId != record.getThreadId())
        {
            return false;
        }
    }
        
    if (bMatchProcessId)
    {
        if (x_MatchProcessId != record.getProcessId())
        {
            return false;
        }
    }
        
    if (bMatchTimeStamp)
    {
        LARGE_INTEGER temp = record.getTimeStamp();
            
        if (memcmp(&x_MatchTimeStamp, &temp, sizeof(LARGE_INTEGER)) != 0)
        {
            return false;
        }
    }
        
    if (bMatchProviderId)
    {
        GUID temp = record.getProviderId();
            
        if (memcmp(&x_MatchProviderId, &temp, sizeof(GUID)) != 0)
        {
            return false;
        }
    }
        
    if (bMatchActivityId)
    {
        GUID temp = record.getActivityId();
            
        if (memcmp(&x_MatchActivityId, &temp, sizeof(GUID)) != 0)
        {
            return false;
        }
    }
        
    if (bMatchKernelTime)
    {
        ULONG temp;
            
        if ( !record.queryKernelTime(&temp) ||
                (x_MatchKernelTime != temp))
        {
            return false;
        }
    }
        
    if (bMatchUserTime)
    {
        ULONG temp;
            
        if ( !record.queryUserTime(&temp) ||
                (x_MatchUserTime != temp))
        {
            return false;
        }
    }
        
    if (bMatchProcessorTime)
    {
        if (x_MatchProcessorTime != record.getProcessorTime())
        {
            return false;
        }
    }
        
    if (bMatchEventId)
    {
        if (x_MatchEventId != record.getEventId())
        {
            return false;
        }
    }
        
    if (bMatchVersion)
    {
        if (x_MatchVersion != record.getVersion())
        {
            return false;
        }
    }
        
    if (bMatchChannel)
    {
        if (x_MatchChannel != record.getChannel())
        {
            return false;
        }
    }
        
    if (bMatchLevel)
    {
        if (x_MatchLevel != record.getLevel())
        {
            return false;
        }
    }
        
    if (bMatchOpcode)
    {
        if (x_MatchOpcode != record.getOpcode())
        {
            return false;
        }
    }
        
    if (bMatchTask)
    {
        if (x_MatchTask != record.getTask())
        {
            return false;
        }
    }
        
    if (bMatchKeyword)
    {
        if (x_MatchKeyword != record.getKeyword())
        {
            return false;
        }
    }
        
    if (bMatchProcessorNumber)
    {
        if (x_MatchProcessorNumber != record.getProcessorNumber())
        {
            return false;
        }
    }
        
    if (bMatchAlignment)
    {
        if (x_MatchAlignment != record.getAlignment())
        {
            return false;
        }
    }
        
    if (bMatchLoggerId)
    {
        if (x_MatchLoggerId != record.getLoggerId())
        {
            return false;
        }
    }
        
    if (bMatchRelatedActivityId)
    {
        GUID temp;
    
        if ( !record.queryRelatedActivityId(&temp) ||
                (memcmp(&x_MatchRelatedActivityId, &temp, sizeof(GUID)) !=0))
        {
            return false;
        }
    }
        
    if (bMatchSID)
    {
        std::vector<BYTE> temp;
        size_t tempSize = 0;
            
        if ( !record.querySID(temp, &tempSize) ||
                (x_MatchSIDSize != tempSize) ||
                (memcmp(x_MatchSID.data(), temp.data(), tempSize) != 0))
        {
            return false;
        }
    }
    
    if (bMatchTerminalSessionId)
    {
        ULONG temp;
            
        if ( !record.queryTerminalSessionId(&temp) ||
                (x_MatchTerminalSessionId != temp))
        {
            return false;
        }
    }
        
    if (bMatchTransactionInstanceId)
    {
        ULONG temp;
            
        if ( !record.queryTransactionInstanceId(&temp) ||
                (x_MatchTransactionInstanceId != temp))
        {
            return false;
        }
    }
        
    if (bMatchTransactionParentInstanceId)
    {
        ULONG temp;
    
        if ( !record.queryTransactionParentInstanceId(&temp) ||
                (x_MatchTransactionParentInstanceId != temp))
        {
            return false;
        }
    }
        
    if (bMatchTransactionParentGuid)
    {
        GUID temp;
            
        if ( !record.queryTransactionParentGuid(&temp) ||
                (memcmp(&x_MatchTransactionParentGuid, &temp, sizeof(GUID)) != 0))
        {
            return false;
        }
    }
        
    if (bMatchProviderGuid)
    {
        GUID temp;
    
        if ( !record.queryProviderGuid(&temp) ||
                (memcmp(&x_MatchProviderGuid, &temp, sizeof(GUID)) != 0))
        {
            return false;
        }
    }
        
    if (bMatchDecodingSource)
    {
        DECODING_SOURCE temp;
            
        if ( !record.queryDecodingSource(&temp) ||
                (memcmp(&x_MatchDecodingSource, &temp, sizeof(DECODING_SOURCE)) != 0))
        {
            return false;
        }
    }
        
    if (bMatchProviderName)
    {
        std::wstring wstemp;
            
        if ( !record.queryProviderName(wstemp) ||
                (lstrcmpiW(x_MatchProviderName.data(), wstemp.c_str()) != 0))
        {
            return false;
        }
    }
        
    if (bMatchLevelName)
    {
        std::wstring wstemp;
            
        if ( !record.queryLevelName(wstemp) ||
                (lstrcmpiW(x_MatchLevelName.data(), wstemp.c_str()) != 0))
        {
            return false;
        }
    }
        
    if (bMatchChannelName)
    {
        std::wstring wstemp;
            
        if ( !record.queryChannelName(wstemp) ||
                (lstrcmpiW(x_MatchChannelName.data(), wstemp.c_str()) != 0))
        {
            return false;
        }
    }
        
    if (bMatchKeywords)
    {
        std::vector<std::wstring> vtemp;
            
        if (!record.queryKeywords(vtemp))
        {
            return false;
        }
        else if (x_MatchKeywords.size() != vtemp.size())
        {
            return false;
        }
        else
        {
            // need all 4 iterators
            //   (begin and end from the member std::vector and temp std::vector)
            std::vector<std::vector<wchar_t>>::const_iterator thisIter = x_MatchKeywords.begin();
            std::vector<std::vector<wchar_t>>::const_iterator thisEnd  = x_MatchKeywords.end();
                
            for (std::vector<std::wstring>::iterator tempIter = vtemp.begin(), tempEnd = vtemp.end();
                    (tempIter != tempEnd) && (thisIter != thisEnd);
                    ++tempIter, ++thisIter)
            {
                const wchar_t* tempString = tempIter->c_str();
                const wchar_t* thisString = thisIter->data();
                if (lstrcmpiW(thisString, tempString) != 0)
                {
                    return false;
                }
            }
        }
    }
        
    if (bMatchTaskName)
    {
        std::wstring wstemp;
            
        if ( !record.queryTaskName(wstemp) ||
                (lstrcmpiW(x_MatchTaskName.data(), wstemp.c_str()) != 0))
        {
            return false;
        }
    }
            
    if (bMatchOpcodeName)
    {
        std::wstring wstemp;
            
        if ( !record.queryOpcodeName(wstemp) ||
                (lstrcmpiW(x_MatchOpcodeName.data(), wstemp.c_str()) != 0))
        {
            return false;
        }
    }
            
    if (bMatchEventMessage)
    {
        std::wstring wstemp;
            
        if ( !record.queryEventMessage(wstemp) ||
                (lstrcmpiW(x_MatchEventMessage.data(), wstemp.c_str()) != 0))
        {
            return false;
        }
    }
            
    if (bMatchProviderMessageName)
    {
        std::wstring wstemp;
            
        if ( !record.queryProviderMessageName(wstemp) ||
                (lstrcmpiW(x_MatchProviderMessageName.data(), wstemp.c_str()) != 0))
        {
            return false;
        }
    }
            
    if (bMatchPropertyCount)
    {
        ULONG temp;
            
        if ( !record.queryPropertyCount(&temp) ||
                (x_MatchPropertyCount != temp))
        {
            return false;
        }
    }
        
    if (bMatchTopLevelPropertyCount)
    {
        ULONG temp;
            
        if ( !record.queryTopLevelPropertyCount(&temp) ||
                (x_MatchTopLevelPropertyCount != temp))
        {
            return false;
        }
    }
        
    if (bMatchProperty)
    {
        std::vector<ntlPropertyQueryPair>::const_iterator iterProperty = x_MatchProperty.begin();
        std::vector<ntlPropertyQueryPair>::const_iterator iterEnd  = x_MatchProperty.end();
            
        std::wstring wsTemp;
        for ( ; iterProperty != iterEnd; ++iterProperty)
        {
            if (!record.queryEventProperty(iterProperty->first.data(), wsTemp))
            {
                return false;
            }
            if (0 != lstrcmpiW(iterProperty->second.data(), wsTemp.c_str()))
            {
                return false;
            }
        }
    }
    return true;
}
    
//
// EVENT_HEADER fields
//
inline
void EtwRecordQuery::matchThreadId(const ULONG & _data) NOEXCEPT
{
    bMatchThreadId = true;
    x_MatchThreadId = _data;
}
inline
void EtwRecordQuery::matchProcessId(const ULONG & _data) NOEXCEPT
{
    bMatchProcessId = true;
    x_MatchProcessId = _data;
}
inline
void EtwRecordQuery::matchTimeStamp(const LARGE_INTEGER & _data) NOEXCEPT
{
    bMatchTimeStamp = true;
    x_MatchTimeStamp = _data;
}
inline
void EtwRecordQuery::matchProviderId(const GUID & _data) NOEXCEPT
{
    bMatchProviderId = true;
    x_MatchProviderId = _data;
}
inline
void EtwRecordQuery::matchActivityId(const GUID & _data) NOEXCEPT
{
    bMatchActivityId = true;
    x_MatchActivityId = _data;
}
inline
void EtwRecordQuery::matchKernelTime(const ULONG & _data) NOEXCEPT
{
    bMatchKernelTime = true;
    x_MatchKernelTime = _data;
}
inline
void EtwRecordQuery::matchUserTime(const ULONG & _data) NOEXCEPT
{
    bMatchUserTime = true;
    x_MatchUserTime = _data;
}
inline
void EtwRecordQuery::matchProcessorTime(const ULONG64 & _data) NOEXCEPT
{
    bMatchProcessorTime = true;
    x_MatchProcessorTime = _data;
}
//
// EVENT_DESCRIPTOR fields
//
inline
void EtwRecordQuery::matchEventId(const USHORT & _data) NOEXCEPT
{
    bMatchEventId = true;
    x_MatchEventId = _data;
}
inline
void EtwRecordQuery::matchVersion(const UCHAR & _data) NOEXCEPT
{
    bMatchVersion = true;
    x_MatchVersion = _data;
}
inline
void EtwRecordQuery::matchChannel(const UCHAR & _data) NOEXCEPT
{
    bMatchChannel = true;
    x_MatchChannel = _data;
}
inline
void EtwRecordQuery::matchLevel(const UCHAR & _data) NOEXCEPT
{
    bMatchLevel = true;
    x_MatchLevel = _data;
}
inline
void EtwRecordQuery::matchOpcode(const UCHAR & _data) NOEXCEPT
{
    bMatchOpcode = true;
    x_MatchOpcode = _data;
}
inline
void EtwRecordQuery::matchTask(const USHORT & _data) NOEXCEPT
{
    bMatchTask = true;
    x_MatchTask = _data;
}
inline
void EtwRecordQuery::matchKeyword(const ULONGLONG & _data) NOEXCEPT
{
    bMatchKeyword = true;
    x_MatchKeyword = _data;
}
//
// ETW_BUFFER_CONTEXT fields
//
inline
void EtwRecordQuery::matchProcessorNumber(const UCHAR & _data) NOEXCEPT
{
    bMatchProcessorNumber = true;
    x_MatchProcessorNumber = _data;
}
inline
void EtwRecordQuery::matchAlignment(const UCHAR & _data) NOEXCEPT
{
    bMatchAlignment = true;
    x_MatchAlignment = _data;
}
inline
void EtwRecordQuery::matchLoggerId(const USHORT & _data) NOEXCEPT
{
    bMatchLoggerId = true;
    x_MatchLoggerId = _data;
}
//
// EVENT_HEADER_EXTENDED_DATA_ITEM options
//
inline
void EtwRecordQuery::matchRelatedActivityId(const GUID & _data) NOEXCEPT
{
    bMatchRelatedActivityId = true;
    x_MatchRelatedActivityId = _data;
}
inline
void EtwRecordQuery::matchSID(_In_reads_bytes_(_dataSize) const BYTE* _data, const size_t _dataSize)
{
    x_MatchSID = std::vector<BYTE>(_dataSize);
    memcpy(x_MatchSID.data(), _data, _dataSize);
    x_MatchSIDSize = _dataSize;
    bMatchSID = true;
}
inline
void EtwRecordQuery::matchTerminalSessionId(const ULONG & _data) NOEXCEPT
{
    bMatchTerminalSessionId = true;
    x_MatchTerminalSessionId = _data;
}
inline
void EtwRecordQuery::matchTransactionInstanceId(const ULONG & _data) NOEXCEPT
{
    bMatchTransactionInstanceId = true;
    x_MatchTransactionInstanceId = _data;
}
inline
void EtwRecordQuery::matchTransactionParentInstanceId(const ULONG & _data) NOEXCEPT
{
    bMatchTransactionParentInstanceId = true;
    x_MatchTransactionParentInstanceId = _data;
}
inline
void EtwRecordQuery::matchTransactionParentGuid(const GUID & _data) NOEXCEPT
{
    bMatchTransactionParentGuid = true;
    x_MatchTransactionParentGuid = _data;
}
//
// TRACE_EVENT_INFO options
//
inline
void EtwRecordQuery::matchProviderGuid(const GUID & _data) NOEXCEPT
{
    bMatchProviderGuid = true;
    x_MatchProviderGuid = _data;
}
inline
void EtwRecordQuery::matchDecodingSource(const DECODING_SOURCE & _data) NOEXCEPT
{
    bMatchDecodingSource = true;
    x_MatchDecodingSource = _data;
}
inline
void EtwRecordQuery::matchProviderName(_In_z_ const wchar_t * _data)
{
    size_t cchBuffer = wcslen(_data) + 1; // +1 for null terminator
    x_MatchProviderName = std::vector<wchar_t>(cchBuffer);
    memcpy(x_MatchProviderName.data(), _data, cchBuffer * sizeof(wchar_t));
    bMatchProviderName = true;
}
inline
void EtwRecordQuery::matchLevelName(_In_z_ const wchar_t * _data)
{
    size_t cchBuffer = wcslen(_data) + 1; // +1 for null terminator
    x_MatchLevelName = std::vector<wchar_t>(cchBuffer);
    memcpy(x_MatchLevelName.data(), _data, cchBuffer * sizeof(wchar_t));
    bMatchLevelName = true;
}
inline
void EtwRecordQuery::matchChannelName(_In_z_ const wchar_t * _data)
{
    size_t cchBuffer = wcslen(_data) + 1; // +1 for null terminator
    x_MatchChannelName = std::vector<wchar_t>(cchBuffer);
    memcpy(x_MatchChannelName.data(), _data, cchBuffer * sizeof(wchar_t));
    bMatchChannelName = true;
}
inline
void EtwRecordQuery::matchKeywords(const std::vector<std::vector<wchar_t>> &  _data)
{
    bMatchKeywords = true;
    x_MatchKeywords = _data;
}
inline
void EtwRecordQuery::matchTaskName(_In_z_ const wchar_t * _data)
{
    size_t cchBuffer = wcslen(_data) + 1; // +1 for null terminator
    x_MatchTaskName = std::vector<wchar_t>(cchBuffer);
    memcpy(x_MatchTaskName.data(), _data, cchBuffer * sizeof(wchar_t));
    bMatchTaskName = true;
}
inline
void EtwRecordQuery::matchOpcodeName(_In_z_ const wchar_t * _data)
{
    size_t cchBuffer = wcslen(_data) + 1; // +1 for null terminator
    x_MatchOpcodeName = std::vector<wchar_t>(cchBuffer);
    memcpy(x_MatchOpcodeName.data(), _data, cchBuffer * sizeof(wchar_t));
    bMatchOpcodeName = true;
}
inline
void EtwRecordQuery::matchEventMessage(_In_z_ const wchar_t * _data)
{
    size_t cchBuffer = wcslen(_data) + 1; // +1 for null terminator
    x_MatchEventMessage = std::vector<wchar_t>(cchBuffer);
    memcpy(x_MatchEventMessage.data(), _data, cchBuffer * sizeof(wchar_t));
    bMatchEventMessage = true;
}
inline
void EtwRecordQuery::matchProviderMessageName(_In_z_ const wchar_t * _data)
{
    size_t cchBuffer = wcslen(_data) + 1; // +1 for null terminator
    x_MatchProviderMessageName = std::vector<wchar_t>(cchBuffer);
    memcpy(x_MatchProviderMessageName.data(), _data, cchBuffer * sizeof(wchar_t));
    bMatchProviderMessageName = true;
}
inline
void EtwRecordQuery::matchPropertyCount(const ULONG & _data) NOEXCEPT
{
    bMatchPropertyCount = true;
    x_MatchPropertyCount = _data;
}
inline
void EtwRecordQuery::matchTopLevelPropertyCount(const ULONG & _data) NOEXCEPT
{
    bMatchTopLevelPropertyCount = true;
    x_MatchTopLevelPropertyCount = _data;
}
inline
void EtwRecordQuery::matchProperty(_In_z_ const wchar_t* _szPropertyName, _In_z_ const wchar_t* _szPropertyValue)
{
    size_t cchPropertyName = wcslen(_szPropertyName) + 1;
    size_t cchPropertyValue = wcslen(_szPropertyValue) + 1;
    std::vector<wchar_t> pPropertyName = std::vector<wchar_t>(cchPropertyName);
    std::vector<wchar_t> pPropertyValue = std::vector<wchar_t>(cchPropertyValue);
    wcscpy_s(
        pPropertyName.data(),
        cchPropertyName,
        _szPropertyName
       );
    wcscpy_s(
        pPropertyValue.data(),
        cchPropertyValue,
        _szPropertyValue
       );
    x_MatchProperty.push_back(std::make_pair(pPropertyName, pPropertyValue));
    bMatchProperty = true;
}
inline
void EtwRecordQuery::matchProperty(_In_z_ const wchar_t* _szAddressName, _In_z_ const wchar_t* _szPortName, _In_ SOCKADDR_STORAGE& _addr)
{
    wchar_t szPort[64] = {0};
    wchar_t szAddress[48] = {0};
    //
    // get the string values for the address and port
    if (_addr.ss_family == AF_INET)
    {
        ::RtlIpv4AddressToStringW(
            &(reinterpret_cast<PSOCKADDR_IN>(&_addr)->sin_addr),
            szAddress
           );
        _itow_s(::ntohs(reinterpret_cast<PSOCKADDR_IN>(&_addr)->sin_port), szPort, 10);
    }
    else
    {
        assert(_addr.ss_family == AF_INET6);
        ::RtlIpv6AddressToStringW(
            &(reinterpret_cast<PSOCKADDR_IN6>(&_addr)->sin6_addr),
            szAddress
           );
        _itow_s(::ntohs(reinterpret_cast<PSOCKADDR_IN6>(&_addr)->sin6_port), szPort, 10);
    }
    this->matchProperty(_szAddressName, szAddress);
    this->matchProperty(_szPortName, szPort);
}
inline
void EtwRecordQuery::matchProperty(_In_z_ const wchar_t* _szPropertyName, const GUID& _guid)
{
    RPC_WSTR pszGuid = NULL;
    RPC_STATUS uuidStatus = ::UuidToStringW(&_guid, &pszGuid);
    if (uuidStatus != RPC_S_OK)
    {
        throw ntl::Exception(uuidStatus, L"UuidToString", L"EtwRecordQuery::matchProperty", false);
    }
    this->matchProperty(_szPropertyName, reinterpret_cast<LPWSTR>(pszGuid));
    ::RpcStringFreeW(&pszGuid);
}

} // namespace ntl


#pragma prefast(pop)

