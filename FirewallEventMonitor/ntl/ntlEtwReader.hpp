// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// CPP Headers
#include <cassert>
#include <wchar.h>
#include <deque>
#include <vector>
#include <string>
#include <exception>
#include <algorithm>

// OS Headers
#include <Windows.h>
#include <guiddef.h>
// these 3 headers needed for evntrace.h
#include <wmistr.h>
#include <winmeta.h>
#include <evntcons.h>
#include <evntrace.h>

// Local project headers
#include "ntlLocks.hpp"
#include "ntlException.hpp"
#include "ntlEtwRecord.hpp"
#include "ntlEtwRecordQuery.hpp"

// Re-defining this flag from Wmi.h to avoid pulling in a bunch of dependencies and conflict
#define EVENT_TRACE_USE_MS_FLUSH_TIMER 0x00000010  // FlushTimer value in milliseconds

namespace ntl
{


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// struct EtwReaderDefaultFilter
//
//  A functor predefined as the default trace filter functor for the real-time event callback
//      in the EtwReader class.
//
//  Returns true for all events (the default policy).
//
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
struct EtwReaderDefaultFilter
{
    EtwReaderDefaultFilter()
    {
    }
    ~EtwReaderDefaultFilter()
    {
    }
        
    bool operator() (const PEVENT_RECORD) const
    {
        return true;
    }
};
    
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// class EtwReader
//
// Encapsulates functioning as both an ETW controller and ETW consumer, allowing callers real-time
//      access to ETW events from providers they specify.  Consumers of this class additionally can
//      provide a policy functor to enable a real-time filter over which events from the provider(s)
//      they've specified they want stored in memory for later comparision.  An ETL file is
//      optionally generated with all events from the subscribed providers.
//
// Along with being able to function as a "controller" (starting/stopping sessions; enabling/
//      disabling providers), the callers of this class can specify ETW event filter objects,
//      detailing the property + property values they want to search.  The class provides find and
//      removal semantics to greatly simplify the searching through non-trivial EVENT_RECORD data
//      structure (and subsequent nested data structures).
//
// Search options available follow a FindFirst / FindNext idiom.  The Find* functions apply an internal
//      cursor to track which events have been searched through thus far.  FindFirstEvent and 
//      FindFirstEventSet 'resets' this internal cursor back to the start of the queue of events
//      and conducts the search.
//
// The Find* functions take one or more EtwRecordQuery objects.  These object declaratively define
//      which fields of the event records to compare against when finding events trace real-time.
//      A EtwRecordQuery object can have as many properties defined as needed, but will apply to
//      one and only one actual event when passed to Find*.
//      (The exception being FindAllMatchingEvents() - this takes a single EtwRecordQuery object, 
//      and returns *all* matching events.
//
//
// Methods exposed are:
//
// StartSession()
//      Creates and starts a trace session with the specified name
//
// StopSession()
//      Stops the event trace session that was started with StartSession()
//
// EnableProviders()
//      Enables the specified ETW providers in the existing trace session.
//
// DisableProviders()
//      Disables the specified ETW providers in the existing trace session.
//
// FindFirstEvent()
//      Searches the stored runtime ETW events for one or more [in] EtwRecordQuery objects
//      and optionally returns the matching records.
//
// FindNextEvent()
//      Searches the stored runtime ETW events for a record matching the [in] EtwRecordQuery
//      object, starting from the record last found from a prior call to FindFirstEvent, FindNextEvent,
//      FindFirstEventSet, or FindNextEventSet.
//      If the find was successful, updates an internal cursor to one-past the event found
//      to use on subsequent calls to Find.
//
// FindFirstEventSet()
//      Searches the stored runtime ETW events for a std::deque of [in] EtwRecordQuery objects
//      and optionally returns the matching records.  Starts the search from the beginning of
//      the event queue.
//      If the find was successful, updates an internal cursor to one-past the last event found
//      to use on subsequent calls to find.
//
// FindNextEventSet()
//      Searches the stored runtime ETW events for one or more [in] EtwRecordQuery objects
//      and optionally returns the matching records.  Starts the search from where the prior
//      call to FindFirstEvent / FindNextEvent / FindFirstEventSet / FindNextEventSet left off.
//      If the find was successful, updates an internal cursor to one-past the last event found
//      to use on subsequent calls to find.
//
// FindAllMatchingEvents()
//      Looks for all instances of an event based on the EtwRecordQuery in the queue 
//      of events.
//
//
// RemoveFirstEvent()
// RemoveNextEvent()
//      Searches the stored runtime ETW events for a specified [in] EtwRecordQuery object,
//      removing it when found.  Optionally returns a copy of the matching record.
//      RemoveFirstEvent starts the search from the beginning of the event queue.
//      RemoveNextEvent starts the search based on the existing cursor position.
//      If a record is removed, the cursor is positioned the record after the removed record.
//
// RemoveEventSet()
//      Searches the stored runtime ETW events for a std::deque of [in] EtwRecordQuery objects
//      and optionally returns the matching records.  Starts the search from the beginning of
//      the event queue.
//      If the find was successful, removes all found events fromt the internal event queue.
//      Additionally, if events are removed, the internal cursor is reset.
//
// FlushEvents()
//      Empties the internal queue of events found through the real-time event policy.
//
// CountEvents()
//      Returns the current number of events queued through the real-time event policy.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T = EtwReaderDefaultFilter, typename L = int (__cdecl *)(const wchar_t *,...)>
class EtwReader
{
public:
    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // Constructor
    //
    //  Using either the default filter policy (EtwReaderDefaultFilter), or
    //      the user-specified filter policy (the functor of type T)
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    EtwReader(L _logger = wprintf);
    EtwReader(T _eventFilter, L _logger = wprintf);
        
    ~EtwReader() NOEXCEPT;
    EtwReader(EtwReader const&) = delete;
    EtwReader& operator=(EtwReader const&) = delete;


    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // StartSession()
    // 
    //  Creates and starts a trace session with the specified name
    //
    //  Arguments:
    //      szSessionName - null-terminated string for the unique name for the new trace session
    //
    //      szFileName - null-terminated string for the ETL trace file to be created
    //                   Optional parameter - pass NULL to not create a trace file
    //
    //      sessionGUID - unique GUID identifying this trace session to all providers
    //
    //      msFlushTimer - value, in milliseconds, of the ETW flush timer
    //                     Optional parameter - default value of 0 means system default is used
    //
    // On Failure: the event session was not able to fully start with the worker thread
    //      processing real-time events.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //      - Win32 failures will throw Exception
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    void StartSession(
        _In_z_ const wchar_t* szSessionName, 
        _In_opt_z_ const wchar_t* szFileName,
        const GUID& _sessionGUID,
        const int msFlushTimer = 0
       );

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // OpenSavedSession()
    // 
    //  Opens a trace session from the specific ETL file
    //
    //  Arguments:
    //      szFileName - null-terminated string for the ETL trace file to be read
    //
    // On Failure: the event session was not able to fully start with the worker thread
    //      processing real-time events.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //      - Win32 failures will throw Exception
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    void OpenSavedSession(
        _In_z_ const wchar_t* szFileName
       );

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // WaitForSession()
    //
    //  Waits on the session's thread handle until the thread exits.
    //
    //  Arguments: None
    //
    // Cannot Fail - Will not throw.
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    void WaitForSession() NOEXCEPT;
    
    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // StopSession()
    //
    //  Stops the event trace session that was started with StartSession()
    //      (and subsequently disables all providers)
    //
    //  Arguments: None
    //
    // Cannot Fail - Will not throw.
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    void StopSession() NOEXCEPT;

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // StopSession()
    //
    //  Stops the event trace session that was started with the provided name
    //      (and subsequently disables all providers)
    //
    //  Arguments:
    //      szSessionName - null-terminated string for the unique name for the trace session
    //
    // On Failure: the event session was not able to be closed (or did not exist)
    //      An exception derived from std::exception will be thrown
    //      - Win32 failures will throw Exception
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    void StopSession(
        _In_z_ const wchar_t* szSessionName);
    
    //////////////////////////////////////////////////////////////////////////////////////////
    // 
    // EnableProviders()
    //
    // Enables the specified ETW providers in the existing trace session.
    // Fails if StartSession() has not been called successfully, or if the worker thread
    //      pumping events has stopped unexpectedly.
    //
    //  Arguments:
    //      providerGUIDs - std::vector of ETW Provider GUIDs that the caller wants enabled in this
    //                       trace session.  An empty std::vector enables no providers.
    //
    //      uLevel - the "Level" parameter passed to EnableTraceEx for providers specified
    //               default is TRACE_LEVEL_VERBOSE (all)
    //
    //      uMatchAnyKeyword - the "MatchAnyKeyword" parameter passed to EnableTraceEx
    //                         default is 0 (none)
    //
    //      uMatchAllKeyword - the "MatchAllKeyword" parameter passed to EnableTraceEx
    //                         default is 0 (none)
    //
    // On Failure: unable to enable all providers.
    //      Note: some providers may have been enabled before the hard-failure occured.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //      - Win32 failures will throw Exception
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    void EnableProviders(
        const std::vector<GUID>& providerGUIDs,
        UCHAR uLevel = TRACE_LEVEL_VERBOSE,
        ULONGLONG uMatchAnyKeyword = 0,
        ULONGLONG uMatchAllKeyword = 0
       );
            
    //////////////////////////////////////////////////////////////////////////////////////////
    // 
    // DisableProviders()
    //
    // Disables the specified ETW providers in the existing trace session.
    // Fails if StartSession() has not been called successfully, or if the worker thread
    //      pumping events has stopped unexpectedly.
    //
    // Arguments:
    //      providerGUIDs - std::vector of ETW Provider GUIDs that the caller wants enabled in this
    //                       trace session.  An empty std::vector enables no providers.
    //
    // On Failure: unable to disable all providers.
    //      Note: some providers may have been disabled before the hard-failure occured.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //      - Win32 failures will throw Exception
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    void DisableProviders(const std::vector<GUID> & providerGUIDs);
        
    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // FindFirstEvent()
    //
    // Searches the stored runtime ETW events for one or more [in] EtwRecordQuery objects
    // and optionally returns the matching records.
    //
    // Arguments:
    //      record_query - a reference to a EtwRecordQuery instance which defines the
    //                     query parameters used to find a match.
    //
    //      found_event - a reference to a EtwRecordQuery which receives a copy of a found
    //                    match. (Optional)
    //
    //      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
    //                      available before returning.
    //
    // Returns:
    //      true - if the event is found in the queue of found events.
    //      false - if the event is not found within the entire queue of found events.
    //
    // On Failure: unable to determine if the event exists in the queue or not.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    bool FindFirstEvent(const ntl::EtwRecordQuery & record_query, unsigned uMilliseconds = 0);
    bool FindFirstEvent(const ntl::EtwRecordQuery & record_query, _Out_ ntl::EtwRecord & found_event, unsigned uMilliseconds = 0);

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // FindNextEvent()
    //
    // Searches the stored runtime ETW events for a record matching the [in] EtwRecordQuery
    // object, starting from the record last found from a prior call to FindFirstEvent, FindNextEvent,
    // FindFirstEventSet, or FindNextEventSet.
    //
    // If the find was successful, updates an internal cursor to one-past the event found
    // to use on subsequent calls to Find.
    //
    // Arguments:
    //      record_query - a reference to a EtwRecordQuery instance which defines the
    //                     query parameters used to find a match.
    //
    //      found_event  - a reference to a EtwRecordQuery objects which 
    //                     receives a copy of records that match the query object.
    //                     (Optional)
    //
    //      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
    //                      available before returning.
    //
    // Returns:
    //      true - if a matching event is found in the queue of found events.
    //      false - if no matching events are found within the entire queue of found events.
    //
    // On Failure: unable to determine if the event exists in the queue or not.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    bool FindNextEvent(const ntl::EtwRecordQuery & record_query, unsigned uMilliseconds = 0);
    bool FindNextEvent(const ntl::EtwRecordQuery & record_query, _Out_ ntl::EtwRecord & found_event, unsigned uMilliseconds = 0);

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // FindFirstEventSet()
    //
    // Searches the stored runtime ETW events for a std::deque of [in] EtwRecordQuery objects
    // and optionally returns the matching records.  Starts the search from the beginning of
    // the event queue.
    //
    // If the find was successful, updates an internal cursor to one-past the last event found
    // to use on subsequent calls to find.
    //
    // Arguments:
    //      record_query_queue - a reference to a std::deque of EtwRecordQuery objects which 
    //                           defines the query parameters of the received events used 
    //                           to find a match.
    //
    //      found_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
    //                          receives a copy of records that match the query objects.
    //                          (Optional)
    //
    //      bRequireInOrder - boolean flag do dictate if the real-time events being searched
    //                        should match the EtwRecordQuery objects only if in identical
    //                        ordering.
    //
    //      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
    //                      available before returning.
    //
    // Returns:
    //      true - if the event is found in the queue of found events.
    //      false - if the event is not found within the entire queue of found events.
    //
    // On Failure: unable to determine if the event exists in the queue or not.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    bool FindFirstEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, bool bRequireInOrder, unsigned uMilliseconds = 0);
    bool FindFirstEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, bool bRequireInOrder, unsigned uMilliseconds = 0);

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // FindNextEventSet()
    //
    // Searches the stored runtime ETW events for one or more [in] EtwRecordQuery objects
    // and optionally returns the matching records.  Starts the search from where the prior
    // call to FindFirstEvent / FindNextEvent / FindFirstEventSet / FindNextEventSet left off.
    //
    // If the find was successful, updates an internal cursor to one-past the last event found
    // to use on subsequent calls to find.
    //
    // Arguments:
    //      record_query_queue - a reference to a std::deque of EtwRecordQuery objects which 
    //                           defines the query parameters of the received events used 
    //                           to find a match.
    //
    //      found_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
    //                          receives a copy of records that match the query objects.
    //                          (Optional)
    //
    //      bRequireInOrder - boolean flag do dictate if the real-time events being searched
    //                        should match the EtwRecordQuery objects only if in identical
    //                        ordering.
    //
    //      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
    //                      available before returning.
    //
    // Returns:
    //      true - if the event is found in the queue of found events.
    //      false - if the event is not found within the entire queue of found events.
    //
    // On Failure: unable to determine if the event exists in the queue or not.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    bool FindNextEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, bool bRequireInOrder, unsigned uMilliseconds = 0);
    bool FindNextEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, bool bRequireInOrder, unsigned uMilliseconds = 0);

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // FindAllMatchingEvents()
    //
    // Searches the stored runtime ETW events for all instances of the [in] EtwRecordQuery 
    // object, and returns a queue of the matching records.
    //
    // Arguments:
    //      record_query - a reference to a EtwRecordQuery instance which defines the
    //                     query parameters used to find a match.
    //
    //      found_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
    //                          receives a copy of records that match the query object.
    //
    // Returns:
    //      true - if at least one matching event is found in the queue of found events.
    //      false - if no matching events are found within the entire queue of found events.
    //
    // On Failure: unable to determine if the event exists in the queue or not.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    bool FindAllMatchingEvents(const ntl::EtwRecordQuery & record_query, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, unsigned uMilliseconds = 0);

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // RemoveFirstEvent()
    // RemoveNextEvent()
    //
    // Searches the stored runtime ETW events for a specified [in] EtwRecordQuery object,
    // removing it when found.  Optionally returns a copy of the matching record.
    //
    // RemoveFirstEvent starts the search from the beginning of the event queue.
    // RemoveNextEvent starts the search based on the existing cursor position.
    // If a record is removed, the cursor is positioned the record after the removed record.
    //
    // Arguments:
    //      record_query - a reference to a EtwRecordQuery object which 
    //                     defines the query parameters of the received events used 
    //                     to find a match.
    //
    //      removed_event - a reference to a EtwRecordQuery object which 
    //                      receives a copy of records that match the query object.
    //                      (Optional)
    //
    //      bRequireInOrder - boolean flag do dictate if the real-time events being searched
    //                        should match the EtwRecordQuery objects only if in identical
    //                        ordering.
    //
    //      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
    //                      available before returning.
    //
    // Returns:
    //      true - if the events are found and removed in the queue of found events.
    //      false - if the events are not found within the entire queue of found events.
    //
    // On Failure: unable to determine if the event exists in the queue or not.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    bool RemoveFirstEvent(const ntl::EtwRecordQuery & record_query, unsigned uMilliseconds = 0);
    bool RemoveFirstEvent(const ntl::EtwRecordQuery & record_query, _Out_ ntl::EtwRecord & found_event, unsigned uMilliseconds = 0);
    bool RemoveNextEvent(const ntl::EtwRecordQuery & record_query, unsigned uMilliseconds = 0);
    bool RemoveNextEvent(const ntl::EtwRecordQuery & record_query, _Out_ ntl::EtwRecord & found_event, unsigned uMilliseconds = 0);

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // RemoveEventSet()
    //
    // Searches the stored runtime ETW events for a std::deque of [in] EtwRecordQuery objects
    // and optionally returns the matching records.  Starts the search from the beginning of
    // the event queue.
    //
    // If the find was successful, removes all found events fromt the internal event queue.
    // Additionally, if events are removed, the internal cursor is reset.
    //
    // Arguments:
    //      record_query_queue - a reference to a std::deque of EtwRecordQuery objects which 
    //                           defines the query parameters of the received events used 
    //                           to find a match.
    //
    //      removed_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
    //                            receives a copy of records that match the query objects.
    //                            (Optional)
    //
    //      bRequireInOrder - boolean flag do dictate if the real-time events being searched
    //                        should match the EtwRecordQuery objects only if in identical
    //                        ordering.
    //
    //      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
    //                      available before returning.
    //
    // Returns:
    //      true - if the events are found and removed in the queue of found events.
    //      false - if the events are not found within the entire queue of found events.
    //
    // On Failure: unable to determine if the event exists in the queue or not.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    bool RemoveEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, bool bRequireInOrder, unsigned uMilliseconds);
    bool RemoveEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, bool bRequireInOrder, unsigned uMilliseconds);

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // RemoveAllMatchingEvents()
    //
    // Searches the stored runtime ETW events for all instances of the [in] EtwRecordQuery 
    // object, removes them, and returns a queue of the matching records.
    //
    // Arguments:
    //      record_query - a reference to a EtwRecordQuery instance which defines the
    //                     query parameters used to find a match.
    //
    //      found_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
    //                          receives a copy of records that match the query object.
    //
    // Returns:
    //      true - if at least one matching event is found in the queue of found events.
    //      false - if no matching events are found within the entire queue of found events.
    //
    // On Failure: unable to determine if the event exists in the queue or not.
    //      An exception derived from std::exception will be thrown
    //      - low resource conditions will throw std::bad_alloc
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    bool RemoveAllMatchingEvents(const ntl::EtwRecordQuery & record_query, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, unsigned uMilliseconds = 0);

    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // FlushEvents()
    //
    // Empties the internal queue of events found through the real-time event policy.
    // Can be called while events are still being processed real-time.
    //
    // Arguments:
    //      foundEvents - an reference to an [out] std::deque to be populated with all events
    //                    found from the real-time even policy. (Optional)
    //
    // Returns:
    //      The number of event records that were flushed
    //
    // Cannot Fail - Will not throw.
    //      - a copy is not made of the events - they are swapped into the provided std::deque,
    //        which cannot fail (no copying required, thus no OOM condition possible).
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    size_t FlushEvents() NOEXCEPT;
    size_t FlushEvents(_Out_ std::deque<ntl::EtwRecord> & foundEvents) NOEXCEPT;
        
    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // CountEvents()
    //
    // Returns the current number of events queued through the real-time event policy.
    // Can be called while events are still being processed real-time.
    //
    // Arguments: None
    //
    // Cannot Fail - Will not throw.
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    size_t CountEvents() NOEXCEPT;
    
    //////////////////////////////////////////////////////////////////////////////////////////
    //
    // FlushSession()
    //
    // Explicitly flushes events from the internal ETW buffers
    // - is called internally when trying to Find or Remove
    // - exposing this publicly for callers who need it in other scenarios
    //
    //////////////////////////////////////////////////////////////////////////////////////////
    void FlushSession();
        
private:

    //////////////////////////////////////////////////////////////////////
    //
    // static functions for worker threads
    //
    //////////////////////////////////////////////////////////////////////
    static DWORD WINAPI ProcessTraceThread(LPVOID lpParameter);
    static VOID  WINAPI EventRecordCallback(PEVENT_RECORD pEventRecord);
    static ULONG WINAPI BufferCallback(PEVENT_TRACE_LOGFILE Buffer);
        
    //////////////////////////////////////////////////////////////////////
    //
    // OpenTrace returns a TRACEHANDLE type, but returns 
    //   INVALID_HANDLE_VALUE on failure
    // TRACEHANDLE and HANDLE are incompatible types
    //   (unless blasting it with a C-cast)
    //
    //////////////////////////////////////////////////////////////////////
    static const TRACEHANDLE TRACE_INVALID_HANDLE_VALUE = 0xffffffffffffffff;
        
    //////////////////////////////////////////////////////////////////////
    //
    // Sleeps time while waiting/probing for expected events
    //
    //////////////////////////////////////////////////////////////////////
    static const unsigned int SLEEP_TIME = 50;
        
    //////////////////////////////////////////////////////////////////////
    //
    // Pass-through to the contained filter class
    //    Called from the worker thread pumping real-time events
    //
    //////////////////////////////////////////////////////////////////////
    bool Filter(const PEVENT_RECORD pevent)
    {
        return eventFilter(pevent);
    };
        
    //////////////////////////////////////////////////////////////////////
    //
    // Attempts to add a record to the internal queue of events
    //
    //////////////////////////////////////////////////////////////////////
    void AddEventRecord(const ntl::EtwRecord& trace) NOEXCEPT
    {
        //
        // CS object enters the CS and leaves when exits scope
        //    e.g. if an exception is thrown from the std::deque
        //
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        eventRecordQueue.push_back(trace);
    };
        
    //////////////////////////////////////////////////////////////////////
    //
    // Encapsulate building up the EVENT_TRACE_PROPERTIES struct
    //
    //////////////////////////////////////////////////////////////////////
    EVENT_TRACE_PROPERTIES* BuildEventTraceProperties(
        _Inout_ std::vector<BYTE>&, 
        _In_z_ const wchar_t* szSessionName, 
        _In_opt_z_  const wchar_t* szFileName,
        const int msFlushTimer
       );
            
    //////////////////////////////////////////////////////////////////////
    //
    // Encapsulate verifying the worker thread executing ProcessTrace
    // - hasn't stopped (which would mean no more events are collected)
    //
    //////////////////////////////////////////////////////////////////////
    void VerifySession();
    
    //////////////////////////////////////////////////////////////////////
    //
    // The implementation of some routines are done in private methods
    // - so that the public methods wouldn't need to duplicate the effort
    //
    //////////////////////////////////////////////////////////////////////
    bool FindNextEventImpl(const ntl::EtwRecordQuery & record_query, _Out_opt_ ntl::EtwRecord * pfound_event, unsigned uMilliseconds);
    bool FindNextEventSetImpl(const std::deque<ntl::EtwRecordQuery> & record_query_queue, _Out_opt_ std::deque<ntl::EtwRecord> * pfound_event_queue, bool bRequireInOrder, unsigned uMilliseconds);
    void OpenTraceImpl(_In_ EVENT_TRACE_LOGFILEW & eventLogfile);

private:
    //
    // declaring common types to make methods less verbose
    //
    typedef std::deque<ntl::EtwRecordQuery>::const_iterator  EtwRecordQueryIterator;
    typedef std::deque<ntl::EtwRecord>::const_iterator       EtwRecordIterator;
    typedef std::deque<ntl::EtwRecord>::difference_type      EtwRecordOffset;
    //
    // member variables
    L logger;
    CRITICAL_SECTION csRecordQueue;
    std::deque<ntl::EtwRecord> eventRecordQueue;
    EtwRecordOffset findCursor;
    T eventFilter;
    TRACEHANDLE sessionHandle;
    TRACEHANDLE traceHandle;
    HANDLE threadHandle;
    GUID sessionGUID;
    bool openSavedSession;
    bool initNumBuffers;
    UINT numBuffers;
};
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  Default Constructor
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
EtwReader<T,L>::EtwReader(L _logger)
: logger(_logger),
    sessionHandle(NULL),
    traceHandle(TRACE_INVALID_HANDLE_VALUE),
    threadHandle(NULL),
    openSavedSession(false),
    initNumBuffers(false)
{
    ::InitializeCriticalSectionEx(&csRecordQueue, 4000, 0);
    ::ZeroMemory(&sessionGUID, sizeof(GUID));
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  Constructor taking a policy functor to filter real-time events
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
EtwReader<T, L>::EtwReader(T _eventFilter, L _logger)
: logger(_logger),
    eventFilter(_eventFilter),
    sessionHandle(NULL),
    traceHandle(TRACE_INVALID_HANDLE_VALUE),
    threadHandle(NULL),
    openSavedSession(false),
    initNumBuffers(false)
{
    ::InitializeCriticalSectionEx(&csRecordQueue, 4000, 0);
    ::ZeroMemory(&sessionGUID, sizeof(GUID));
}
    
////////////////////////////////////////////////////////////////////////////////
//
//  Destructor
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
EtwReader<T, L>::~EtwReader() NOEXCEPT
{
    StopSession();
    ::DeleteCriticalSection(&csRecordQueue);
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  StartSession
//
// Arguments:
//      szSessionName - null-terminated string for the unique name for the new trace session
//      szFileName - null-terminated string for the ETL trace file to be created
//                   Optional parameter - pass NULL to not create a trace file
//      sessionGUID - unique GUID identifying this trace session to all providers
//
//
// On Failure: the event session was not able to fully start with the worker thread
//      processing real-time events.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//      - Win32 failures will throw Exception
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
void EtwReader<T, L>::StartSession(_In_z_ const wchar_t* szSessionName, _In_opt_z_  const wchar_t* szFileName, const GUID& _sessionGUID, const int msFlushTimer)
{
    //
    // block improper reentrency
    //
    if ( (sessionHandle != NULL) ||
            (traceHandle != TRACE_INVALID_HANDLE_VALUE) ||
            (threadHandle != NULL))
    {
        logger(L"\tEtwReader::StartSession is called while a session is already started\n");
        throw std::runtime_error("EtwReader::StartSession is called while a session is already started");
    }
    //
    // Need a copy of the session GUID to enable/disable providers
    //
    sessionGUID = _sessionGUID;
    //
    // allocate the buffer for the EVENT_TRACE_PROPERTIES structure
    // - plus the session name and file name appended to the end of the struct
    //
    std::vector<BYTE> pPropertyBuffer;
    EVENT_TRACE_PROPERTIES* pProperties = BuildEventTraceProperties(pPropertyBuffer, szSessionName, szFileName, msFlushTimer);
    //
    // Now start the trace session
    //
    ULONG ulReturn = ::StartTraceW(
        &(sessionHandle),    // session handle
        szSessionName,             // session name
        pProperties                // trace properties struct
       );
    if (ERROR_ALREADY_EXISTS == ulReturn)
    {
        logger(L"\tEtwReader::StartSession - session with the name %s is already running - stopping/restarting that session\n", szSessionName);
        //
        // Try to stop the session by its session name
        //
        EVENT_TRACE_PROPERTIES tempProperties;
        ::ZeroMemory(reinterpret_cast<void*>(&tempProperties), sizeof(EVENT_TRACE_PROPERTIES));
        tempProperties.Wnode.BufferSize = sizeof(EVENT_TRACE_PROPERTIES);
        ::ControlTraceW(NULL, szSessionName, &tempProperties, EVENT_TRACE_CONTROL_STOP);
        //
        // Try to start the session again
        //
        ulReturn = ::StartTraceW(
            &(sessionHandle),    // session handle
            szSessionName,             // session name
            pProperties                // trace properties struct
           );
    }
    if (ulReturn != ERROR_SUCCESS)
    {
        logger(L"\tEtwReader::StartSession - StartTrace failed with error 0x%x\n", ulReturn);
        throw ntl::Exception(ulReturn, L"StartTrace", L"EtwReader::StartSession", false);
    }
    //
    // forced to make a copy of szSessionName in a non-const string for the EVENT_TRACE_LOGFILE.LoggerName member
    //
    size_t sessionSize = wcslen(szSessionName) + 1;
    std::vector<wchar_t> localSessionName (sessionSize);
    ::wcscpy_s(localSessionName.data(), sessionSize, szSessionName);
    localSessionName[sessionSize-1] = L'\0';
    //
    // Setup the EVENT_TRACE_LOGFILE to prepare the callback for real-time notification
    //
    EVENT_TRACE_LOGFILEW eventLogfile;
    ::ZeroMemory(&eventLogfile, sizeof(EVENT_TRACE_LOGFILE));
    eventLogfile.LogFileName = NULL;
    eventLogfile.LoggerName = localSessionName.data();
    eventLogfile.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    eventLogfile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;
    eventLogfile.BufferCallback = NULL;
    eventLogfile.EventCallback = NULL;
    eventLogfile.EventRecordCallback = EventRecordCallback;
    eventLogfile.Context = reinterpret_cast<VOID*>(this);// a PVOID to pass to the callback function
    OpenTraceImpl(eventLogfile);
}
template <typename T, typename L>
void EtwReader<T, L>::OpenSavedSession(_In_z_ const wchar_t* szFileName)
{
    //
    // block improper reentrency
    //
    if ( (sessionHandle != NULL) ||
            (traceHandle != TRACE_INVALID_HANDLE_VALUE) ||
            (threadHandle != NULL))
    {
        logger(L"\tEtwReader::StartSession is called while a session is already started\n");
        throw std::runtime_error("EtwReader::StartSession is called while a session is already started");
    }
    openSavedSession = true;
    //
    // forced to make a copy of szFileName in a non-const string for the EVENT_TRACE_LOGFILE.LogFileName member
    //
    size_t fileNameSize = wcslen(szFileName) + 1;
    std::vector<wchar_t> localFileName(fileNameSize);
    ::wcscpy_s(localFileName.data(), fileNameSize, szFileName);
    localFileName[fileNameSize -1] = L'\0';
    //
    // Setup the EVENT_TRACE_LOGFILE to prepare the callback for real-time notification
    //
    EVENT_TRACE_LOGFILEW eventLogfile;
    ::ZeroMemory(&eventLogfile, sizeof(EVENT_TRACE_LOGFILE));
    eventLogfile.LogFileName = localFileName.data();
    eventLogfile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD;
    eventLogfile.BufferCallback = BufferCallback;
    eventLogfile.EventCallback = NULL;
    eventLogfile.EventRecordCallback = EventRecordCallback;
    eventLogfile.Context = reinterpret_cast<VOID*>(this);// a PVOID to pass to the callback function
    OpenTraceImpl(eventLogfile);
}

template <typename T, typename L>
void EtwReader<T, L>::OpenTraceImpl(_In_ EVENT_TRACE_LOGFILEW & eventLogfile)
{
    //
    // Open a trace handle to start receiving events on the callback
    // - need to define "Invalide handle value" as a TRACEHANDLE,
    //   since they are different types (ULONG64 versus void*)
    //
    traceHandle = ::OpenTraceW(&eventLogfile);
    if (traceHandle == TRACE_INVALID_HANDLE_VALUE)
    {
        DWORD gle = ::GetLastError();
        logger(L"\tEtwReader::StartSession - OpenTrace failed with error 0x%x\n", gle);
        throw ntl::Exception(gle, L"OpenTrace", L"EtwReader::StartSession", false);
    }
    threadHandle = ::CreateThread(
        NULL,
        0,
        ProcessTraceThread,
        &(traceHandle),
        0,
        NULL
   );
    if (NULL == threadHandle)
    {
        DWORD gle = ::GetLastError();
        logger(L"\tEtwReader::StartSession - CreateThread failed with error 0x%x\n", gle);
        throw ntl::Exception(gle, L"CreateThread", L"EtwReader::StartSession", false);
    }
    //
    // Quick check to see that the worker tread calling ProcessTrace didn't fail out
    //
    VerifySession();
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
//  VerifySession
//
// Arguments: None
//
// On Failure:
//      - Win32 failures will throw Exception
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
void EtwReader<T, L>::VerifySession()
{
    //
    // traceHandle will be reset after the user explicitly calls StopSession(),
    // - so only verify the thread if the user hasn't stoppped the session
    //
    if ((traceHandle != TRACE_INVALID_HANDLE_VALUE) && (threadHandle != NULL))
    {
        //
        // Quick check to see that the worker tread calling ProcessTrace didn't fail out
        //
        DWORD dwWait = ::WaitForSingleObject(threadHandle, 0);
        if (WAIT_OBJECT_0 == dwWait)
        {
            //
            // The worker thread already exited - ProcessTrace() failed
            //
            DWORD dwError = 0;
            ntl::Exception eThrow;
            if (::GetExitCodeThread(threadHandle, &dwError))
            {
                logger(L"\tEtwReader::VerifySession - the ProcessTrace worker thread exited with error 0x%x\n", dwError);
                eThrow = ntl::Exception(dwError, L"ProcessTrace", L"EtwReader", false);
            }
            else
            {
                DWORD gle = ::GetLastError();
                logger(L"\tEtwReader::VerifySession - the ProcessTrace worker thread exited, but GetExitCodeThread failed with error 0x%x\n", gle);
                eThrow = ntl::Exception(gle, L"GetExitCodeThread", L"EtwReader", false);
            }
            //
            // Close the thread handle now that it's dead
            //
            CloseHandle(threadHandle);
            threadHandle = NULL;
            throw eThrow;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
//  WaitForSession
//
// Arguments: None
//
// Cannot Fail - Will not throw.
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
void EtwReader<T, L>::WaitForSession() NOEXCEPT
{
    if (threadHandle != NULL)
    {
        DWORD dwWait = ::WaitForSingleObject(threadHandle, INFINITE);
        FatalCondition(
            dwWait != WAIT_OBJECT_0,
            L"Failed waiting on EtwReader::StopSession thread to stop [%u - gle %u]",
            dwWait, ::GetLastError());
    }
}
    
////////////////////////////////////////////////////////////////////////////////
//
//  StopSession
//
// Arguments: None
//
// Cannot Fail - Will not throw.
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
void EtwReader<T, L>::StopSession() NOEXCEPT
{
    //
    // initialize the EVENT_TRACE_PROPERTIES struct to stop the session
    //
    if (sessionHandle != NULL)
    {
        EVENT_TRACE_PROPERTIES tempProperties;
        ::ZeroMemory(&tempProperties, sizeof(EVENT_TRACE_PROPERTIES));
        tempProperties.Wnode.BufferSize = sizeof(EVENT_TRACE_PROPERTIES);
        tempProperties.Wnode.Guid = sessionGUID;
        tempProperties.Wnode.ClientContext = 1; // QPC
        tempProperties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        //
        // Stop the session
        //
        ULONG ulReturn = ::ControlTrace(
            sessionHandle,
            NULL,
            &tempProperties,
            EVENT_TRACE_CONTROL_STOP
           );
        //
        // stops the session even when returned ERROR_MORE_DATA
        // - if this fails, there's nothing we can do to compensate
        //
        FatalCondition(
            (ulReturn != ERROR_MORE_DATA) && (ulReturn != ERROR_SUCCESS),
            L"EtwReader::StopSession - ControlTrace failed [%u] : cannot stop the trace session",
            ulReturn);
        sessionHandle = NULL;
    }
    //
    // Close the handle from OpenTrace
    //
    if (traceHandle != TRACE_INVALID_HANDLE_VALUE)
    {
        //
        // ProcessTrace is still unblocked and returns success when
        //    ERROR_CTX_CLOSE_PENDING is returned
        //
        DWORD error = ::CloseTrace(traceHandle);
        FatalCondition(
            (ERROR_SUCCESS != error) && (ERROR_CTX_CLOSE_PENDING != error),
            L"CloseTrace failed [%u] - thus will not unblock the APC thread processing events",
            error);
        traceHandle = TRACE_INVALID_HANDLE_VALUE;
    }
    //
    // the above call to CloseTrace should exit the thread
    //
    if (threadHandle != NULL)
    {
        DWORD dwWait = ::WaitForSingleObject(threadHandle, INFINITE);
        FatalCondition(
            dwWait != WAIT_OBJECT_0,
            L"Failed waiting on EtwReader::StopSession thread to stop [%u - gle %u]",
            dwWait, ::GetLastError());
        ::CloseHandle(threadHandle);
        threadHandle = NULL;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// StopSession()
//
//  Stops the event trace session that was started with the provided name
//      (and subsequently disables all providers)
//
//  Arguments:
//      szSessionName - null-terminated string for the unique name for the trace session
//
// On Failure: the event session was not able to be closed (or did not exist)
//      An exception derived from std::exception will be thrown
//      - Win32 failures will throw Exception
//
//////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
void EtwReader<T, L>::StopSession(_In_z_ const wchar_t* szSessionName)
{
    EVENT_TRACE_PROPERTIES tempProperties;
    ::ZeroMemory(reinterpret_cast<void*>(&tempProperties), sizeof(EVENT_TRACE_PROPERTIES));
    tempProperties.Wnode.BufferSize = sizeof(EVENT_TRACE_PROPERTIES);
    ULONG ulReturn = ::ControlTrace(NULL, szSessionName, &tempProperties, EVENT_TRACE_CONTROL_STOP);
    if ((ulReturn != ERROR_MORE_DATA) && (ulReturn != ERROR_SUCCESS)) {
        throw ntl::Exception(ulReturn, L"ControlTrace", L"EtwReader::StopSession", false);
    }
}


////////////////////////////////////////////////////////////////////////////////
//
// EnableProviders()
//
// Arguments:
//      providerGUIDs - std::vector of ETW Provider GUIDs that the caller wants enabled in this
//                       trace session.  An empty std::vector enables no providers.
//
//      uLevel - the "Level" parameter passed to EnableTraceEx for providers specified
//               default is TRACE_LEVEL_VERBOSE (all)
//
//      uMatchAnyKeyword - the "MatchAnyKeyword" parameter passed to EnableTraceEx
//                         default is 0 (none)
//
//      uMatchAllKeyword - the "MatchAllKeyword" parameter passed to EnableTraceEx
//                         default is 0 (none)
//
// On Failure: unable to enable all providers.
//      Note: some providers may have been enabled before the hard-failure occured.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//      - Win32 failures will throw Exception
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
void EtwReader<T, L>::EnableProviders(const std::vector<GUID>& providerGUIDs, UCHAR uLevel, ULONGLONG uMatchAnyKeyword, ULONGLONG uMatchAllKeyword)
{
    //
    // Block calling if an open session is not running
    //
    VerifySession();
    //
    // iterate through the std::vector of GUIDs, enabling each provider
    //
    for ( std::vector<GUID>::const_iterator guidIter = providerGUIDs.begin(), guidEnd = providerGUIDs.end();
            guidIter != guidEnd;
            ++guidIter)
    {
        const GUID providerGUID = *guidIter;
        ULONG ulReturn = ::EnableTraceEx(
            &providerGUID,  
            &(sessionGUID),
            sessionHandle,
            TRUE,
            uLevel,
            uMatchAnyKeyword,
            uMatchAllKeyword,
            0,
            NULL
           );
        if (ulReturn != ERROR_SUCCESS)
        {
            logger(L"\tEtwReader::EnableProviders - EnableTraceEx failed with error 0x%x\n", ulReturn);
            throw ntl::Exception(ulReturn, L"EnableTraceEx", L"EtwReader::EnableProviders", false);
        }
    }
        
}


////////////////////////////////////////////////////////////////////////////////
//
//  BuildEventTraceProperties
//
// Arguments: None
//      pPropertyBuffer - vector of BYTEs to be populated with
//                        expected data for a EVENT_TRACE_PROPERTIES struct.
//      szSessionName - null-terminated string for the unique name for the new trace session
//      szFileName - null-terminated string for the ETL trace file to be created
//                   Optional parameter - pass NULL to not create a trace file
//
//      msFlushTimer - value, in milliseconds, of the ETW flush timer
//                     Optional parameter - default value of 0 means system default is used
//
// Returns: a pointer to the beginning of the pPropertyBuffer, which points to
//          the expected EVENT_TRACE_PROPERTIES structure.
//
// On Failure: on OOM conditions.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
EVENT_TRACE_PROPERTIES* 
EtwReader<T, L>::BuildEventTraceProperties(_Inout_ std::vector<BYTE>& pPropertyBuffer, _In_z_ const wchar_t* szSessionName, _In_opt_z_  const wchar_t* szFileName, const int msFlushTimer)
{
    //
    // Get buffer sizes in bytes and characters
    //     +1 for null-terminators
    //
    size_t cchSessionLength = ::wcslen(szSessionName) + 1;
    size_t cbSessionSize = cchSessionLength * sizeof(wchar_t);
    if (cbSessionSize < cchSessionLength)
    {
        logger(L"\tEtwReader::BuildEventTraceProperties - the szSessionName was a bad argument\n");
        throw std::runtime_error("Overflow passing Session string to EtwReader");
    }
    size_t cchFileNameLength = 0;
    size_t cbFileNameSize = 0;
    if (szFileName != NULL)
    {
        cchFileNameLength = ::wcslen(szFileName) + 1;
    }
    cbFileNameSize = cchFileNameLength * sizeof(wchar_t);
    if (cbFileNameSize < cchFileNameLength)
    {
        logger(L"\tEtwReader::BuildEventTraceProperties - the szFileName was a bad argument\n");
        throw std::runtime_error("Overflow passing Filename string to EtwReader");
    }
        
    size_t cbProperties = sizeof(EVENT_TRACE_PROPERTIES)+ cbSessionSize + cbFileNameSize;
    pPropertyBuffer = std::vector<BYTE>(cbProperties);
    ::ZeroMemory(pPropertyBuffer.data(), cbProperties);
    //
    // Append the strings to the end of the struct
    //
    if (szFileName != NULL)
    {
        // append the filename to the end of the struct
        ::CopyMemory(
            pPropertyBuffer.data() + sizeof(EVENT_TRACE_PROPERTIES),
            reinterpret_cast<const void*>(szFileName),
            cbFileNameSize
           );
        // append the session name to the end of the struct
        ::CopyMemory(
            pPropertyBuffer.data() + sizeof(EVENT_TRACE_PROPERTIES) + cbFileNameSize,
            reinterpret_cast<const void*>(szSessionName),
            cbSessionSize
           );
    }
    else
    {
        // append the session name to the end of the struct
        ::CopyMemory(
            pPropertyBuffer.data() + sizeof(EVENT_TRACE_PROPERTIES),
            reinterpret_cast<const void*>(szSessionName),
            cbSessionSize
           );
    }
    //
    // Set the required fields for starting a new session:
    //   Wnode.BufferSize 
    //   Wnode.Guid 
    //   Wnode.ClientContext 
    //   Wnode.Flags 
    //   LogFileMode 
    //   LogFileNameOffset 
    //   LoggerNameOffset 
    //
    EVENT_TRACE_PROPERTIES* pProperties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(pPropertyBuffer.data());
    pProperties->MinimumBuffers = 1; // smaller will make it easier to flush - explicitly not performance sensative
    pProperties->Wnode.BufferSize = static_cast<ULONG>(cbProperties);
    pProperties->Wnode.Guid = sessionGUID;
    pProperties->Wnode.ClientContext = 1; // QPC
    pProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    pProperties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    pProperties->LogFileNameOffset = (NULL == szFileName) ? 
        0 : 
        static_cast<ULONG>(sizeof(EVENT_TRACE_PROPERTIES));
    pProperties->LoggerNameOffset = (NULL == szFileName) ? 
        static_cast<ULONG>(sizeof(EVENT_TRACE_PROPERTIES)) : 
        static_cast<ULONG>(sizeof(EVENT_TRACE_PROPERTIES) + cbFileNameSize);
    if (msFlushTimer != 0) {
        pProperties->LogFileMode |= EVENT_TRACE_USE_MS_FLUSH_TIMER;
        pProperties->FlushTimer = msFlushTimer;
    }
    
    return pProperties;
}

    

////////////////////////////////////////////////////////////////////////////////
//
// DisableProviders()
//
// Arguments:
//      providerGUIDs - std::vector of ETW Provider GUIDs that the caller wants enabled in this
//                       trace session.  An empty std::vector enables no providers.
//
// On Failure: unable to disable all providers.
//      Note: some providers may have been disabled before the hard-failure occured.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//      - Win32 failures will throw Exception
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
void EtwReader<T, L>::DisableProviders(const std::vector<GUID>& providerGUIDs)
{
    //
    // Block calling if an open session is not running
    //
    VerifySession();
    //
    // iterate through the std::vector of GUIDs, disabling each provider
    //
    for ( std::vector<GUID>::const_iterator guidIter = providerGUIDs.begin(), guidEnd = providerGUIDs.end();
            guidIter != guidEnd;
            ++guidIter)
    {
        const GUID providerGUID = *guidIter;
        ULONG ulReturn = ::EnableTraceEx(
            &providerGUID,  
            &(sessionGUID),
            sessionHandle,
            FALSE,
            0,
            0,
            0,
            0,
            NULL
           );
        if (ulReturn != ERROR_SUCCESS)
        {
            logger(L"\tEtwReader::DisableProviders - EnableTraceEx failed with error 0x%x\n", ulReturn);
            throw ntl::Exception(ulReturn, L"EnableTraceEx", L"EtwReader::DisableProviders", false);
        }
    }
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
// FlushSession()
//
// Arguments: None
//
// On Failure: unable to flush the events via ControlTrace()
//      An Exception is thrown capturing the Win32 failure
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
void EtwReader<T, L>::FlushSession()
{
    if (sessionHandle != NULL)
    {
        EVENT_TRACE_PROPERTIES tempProperties;
        ::ZeroMemory(&tempProperties, sizeof(EVENT_TRACE_PROPERTIES));
        tempProperties.Wnode.BufferSize = sizeof(EVENT_TRACE_PROPERTIES);
        tempProperties.Wnode.Guid = sessionGUID;
        tempProperties.Wnode.ClientContext = 1; // QPC
        tempProperties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        //
        // Stop the session
        //
        ULONG ulReturn = ::ControlTrace(
            sessionHandle,
            NULL,
            &tempProperties,
            EVENT_TRACE_CONTROL_FLUSH
           );
        //
        // stops the session even when returned ERROR_MORE_DATA
        // - if this fails, there's nothing we can do to compensate
        //
        if((ulReturn != ERROR_MORE_DATA) && (ulReturn != ERROR_SUCCESS))
        {
            logger(L"\tEtwReader::FlushSession - ControlTrace failed with error 0x%x\n", ulReturn);
            throw ntl::Exception(ulReturn, L"ControlTrace", L"EtwReader::FlushSession", false);
        }
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// FindFirstEvent()
//
// Searches the stored runtime ETW events for the specified [in] EtwRecordQuery object
// and optionally returns the matching records.
//
// If the find was successful, updates an internal cursor to one-past the last event found
// to use on subsequent calls to find.
//
// Arguments:
//      record_query - a reference to a EtwRecordQuery instance which defines the
//                     query parameters used to find a match.
//
//      found_event - a reference to a EtwRecordQuery which receives a copy of a found
//                    match. (Optional)
//
//      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
//                      available before returning.
//
// Returns:
//      true - if the event is found in the queue of found events.
//      false - if the event is not found within the entire queue of found events.
//
// On Failure: unable to determine if the event exists in the queue or not.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//
//////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
bool EtwReader<T, L>::FindFirstEvent(const ntl::EtwRecordQuery & record_query, unsigned uMilliseconds)
{
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        //
        // reset the internal cursor to the beginning and do a find
        //
        findCursor = 0;
    }

    return (FindNextEventImpl(record_query, NULL, uMilliseconds));
}
template <typename T, typename L>
bool EtwReader<T, L>::FindFirstEvent(const ntl::EtwRecordQuery & record_query, _Out_ ntl::EtwRecord & found_event, unsigned uMilliseconds)
{
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        //
        // reset the internal cursor to the beginning and do a find
        //
        findCursor = 0;
    }

    return (FindNextEventImpl(record_query, &(found_event), uMilliseconds));
}
//////////////////////////////////////////////////////////////////////////////////////////
//
// FindNextEvent()
//
// Searches the stored runtime ETW events for a record matching the [in] EtwRecordQuery
// object, starting from the record last found from a prior call to FindFirstEvent or FindNextEvent.
//
// If the find was successful, updates an internal cursor to one-past the last event found
// to use on subsequent calls to find.
//
// Arguments:
//      record_query - a reference to a EtwRecordQuery instance which defines the
//                     query parameters used to find a match.
//
//      found_event - a reference to a EtwRecordQuery object which receives
//                     a copy of records that match the query object.
//                     (Optional)
//
//      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
//                      available before returning.
//
// Returns:
//      true - if a matching event is found in the queue of found events.
//      false - if no matching events are found within the entire queue of found events.
//
// On Failure: unable to determine if the event exists in the queue or not.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//
//////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
bool EtwReader<T, L>::FindNextEvent(const ntl::EtwRecordQuery & record_query, unsigned uMilliseconds)
{
    return FindNextEventImpl(record_query, NULL, uMilliseconds);
}
template <typename T, typename L>
bool EtwReader<T, L>::FindNextEvent(const ntl::EtwRecordQuery & record_query, _Out_ ntl::EtwRecord & found_event, unsigned uMilliseconds)
{
    return FindNextEventImpl(record_query, &(found_event), uMilliseconds);
}

template <typename T, typename L>
bool EtwReader<T, L>::FindNextEventImpl(const ntl::EtwRecordQuery & record_query, _Out_opt_ ntl::EtwRecord * pfound_event, unsigned uMilliseconds)
{
    //
    // Using the EtwRecordQueryPredicate to use the std::find_if() algorithm
    //
    ntl::EtwRecordQueryPredicate predicate(record_query);

    EtwRecordOffset localCursor = 0;
    EtwRecordOffset totalRecords = 0;
    DWORD startTime = ::GetTickCount();
    bool bFoundRecord = false;
    DWORD foundID = 0;
    while (!bFoundRecord)
    {
        //
        // explicitly flush events from the ETW internal buffers every iteration
        //
        FlushSession();

        {
            ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
            localCursor = findCursor;

            EtwRecordIterator recordBegin = eventRecordQueue.begin();
            EtwRecordIterator recordEnd   = eventRecordQueue.end();
            //
            // search only if the localCursor isn't pointed at the end()
            // - it will point at the end() when the immediately-prior search failed
            // - but time did not yet expire
            if (localCursor < (recordEnd - recordBegin))
            {
                //
                // search for a matching event, starting with the localCursor offset
                EtwRecordIterator recordIter = std::find_if(
                    recordBegin + localCursor,
                    recordEnd,
                    predicate
                   );
                if (recordIter != recordEnd)
                {
                    bFoundRecord = true;
                    //
                    // update the localCursor to one past the found event unless we delete it
                    localCursor = recordIter - recordBegin + 1;
                    //
                    // save the record if found the desired one - if asked for the return
                    if (pfound_event != NULL)
                    {
                        *pfound_event = *recordIter;
                    }
                    //
                    // get the size and ID while under the lock
                    totalRecords = eventRecordQueue.size();
                    foundID = recordIter->getEventId();
                }
                else
                {
                    bFoundRecord = false;
                    //
                    // update the localCursor to the end() since we didn't fine the record
                    localCursor = recordEnd - recordBegin;
                }
            }
            else
            {
                assert((recordBegin + localCursor) == recordEnd);
            }
        }

        if (!bFoundRecord)
        {
            //
            // if time expired break out now, else sleep
            DWORD currentTime = ::GetTickCount();
            DWORD elapsedTime = (currentTime < startTime) ? // did tick count rollover?
                currentTime + (MAXDWORD - startTime) : // rolled over, so calculate the difference
                currentTime - startTime;  // didn't roll over
            if (elapsedTime > uMilliseconds)
            {
                break;
            }
            else
            {
                ::Sleep(SLEEP_TIME);
            }
        }
    }
    //
    // update the object's cursor if found a record
    //
    if (bFoundRecord)
    {
        assert(localCursor > findCursor);
        findCursor = localCursor;
        logger(L"\tEtwReader::FindEvent found event ID %d at offset %d from a total of %d events\n", foundID, localCursor, totalRecords);
    }
    else
    {
        try
        {
            std::wstring wsText;
            record_query.writeQuery(wsText);
            logger(L"\tEtwReader::FindEvent failed to find the query: %s\n", wsText.c_str());
        }
        catch (const std::exception&)
        {
            // don't fail the call just from failure to write to a logger
        }
    }
    return bFoundRecord;
}
//////////////////////////////////////////////////////////////////////////////////////////
//
// RemoveFirstEvent()
// RemoveNextEvent()
//
// Searches the stored runtime ETW events for a specified [in] EtwRecordQuery object,
// removing it when found.  Optionally returns a copy of the matching record.
//
// RemoveFirstEvent starts the search from the beginning of the event queue.
// RemoveNextEvent starts the search based on the existing cursor position.
// If a record is removed, the cursor is positioned the record after the removed record.
//
// Arguments:
//      record_query - a reference to a EtwRecordQuery object which 
//                     defines the query parameters of the received events used 
//                     to find a match.
//
//      removed_event - a reference to a EtwRecordQuery object which 
//                      receives a copy of records that match the query objects.
//                      (Optional)
//
//      bRequireInOrder - boolean flag do dictate if the real-time events being searched
//                        should match the EtwRecordQuery objects only if in identical
//                        ordering.
//
//      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
//                      available before returning.
//
// Returns:
//      true - if the events are found and removed in the queue of found events.
//      false - if the events are not found within the entire queue of found events.
//
// On Failure: unable to determine if the event exists in the queue or not.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//
//////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
bool EtwReader<T, L>::RemoveFirstEvent(const ntl::EtwRecordQuery & record_query, unsigned uMilliseconds)
{
    ntl::EtwRecord tempRecord;
    //
    // always search from the beginning of the queue for RemoveFirstEvent
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        findCursor = 0;
    }

    bool bReturn = FindNextEventImpl(record_query, &(tempRecord), uMilliseconds);
    if (bReturn)
    {
        {
            ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
            //
            // erase-remove idiom to efficiently remove the element
            eventRecordQueue.erase(
                std::remove(eventRecordQueue.begin(), eventRecordQueue.end(), tempRecord),
                eventRecordQueue.end()
               );
            if (findCursor > 0)
            {
                //
                // decrement if found any beyond the first event
                --findCursor;
            }
            logger(L"\tEtwReader::RemoveFirstEvent - removed 1 record (Event Id %d), leaving %d events\n", tempRecord.getEventId(), eventRecordQueue.size());
        }
    }
    return bReturn;
}
template <typename T, typename L>
bool EtwReader<T, L>::RemoveFirstEvent(const ntl::EtwRecordQuery & record_query, _Out_ ntl::EtwRecord & found_event, unsigned uMilliseconds)
{
    ntl::EtwRecord tempRecord;
    //
    // always search from the beginning of the queue for RemoveFirstEvent
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        findCursor = 0;
    }

    bool bReturn = FindNextEventImpl(record_query, &(tempRecord), uMilliseconds);
    if (bReturn)
    {
        {
            ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
            //
            // erase-remove idiom to efficiently remove the element
            eventRecordQueue.erase(
                std::remove(eventRecordQueue.begin(), eventRecordQueue.end(), tempRecord),
                eventRecordQueue.end()
               );
            if (findCursor > 0)
            {
                //
                // decrement if found any beyond the first event
                --findCursor;
            }
            logger(L"\tEtwReader::RemoveFirstEvent - removed 1 record (Event Id %d), leaving %d events\n", tempRecord.getEventId(), eventRecordQueue.size());
        }
    }
    found_event.swap(tempRecord);
    return bReturn;
}
template <typename T, typename L>
bool EtwReader<T, L>::RemoveNextEvent(const ntl::EtwRecordQuery & record_query, unsigned uMilliseconds)
{
    ntl::EtwRecord tempRecord;
    bool bReturn = FindNextEventImpl(record_query, &(tempRecord), uMilliseconds);
    if (bReturn)
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        csScoped.Enter();
        {
            //
            // erase-remove idiom to efficiently remove the element
            eventRecordQueue.erase(
                std::remove(eventRecordQueue.begin(), eventRecordQueue.end(), tempRecord),
                eventRecordQueue.end()
               );
            if (findCursor > 0)
            {
                //
                // decrement if found any beyond the first event
                --findCursor;
            }
            logger(L"\tEtwReader::RemoveNextEvent - removed 1 record (Event Id %d), leaving %d events\n", tempRecord.getEventId(), eventRecordQueue.size());
        }
        csScoped.Leave();
    }
    return bReturn;
}
template <typename T, typename L>
bool EtwReader<T, L>::RemoveNextEvent(const ntl::EtwRecordQuery & record_query, _Out_ ntl::EtwRecord & found_event, unsigned uMilliseconds)
{
    ntl::EtwRecord tempRecord;
    bool bReturn = FindNextEventImpl(record_query, &(tempRecord), uMilliseconds);
    if (bReturn)
    {
        {
            ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
            //
            // erase-remove idiom to efficiently remove the element
            eventRecordQueue.erase(
                std::remove(eventRecordQueue.begin(), eventRecordQueue.end(), tempRecord),
                eventRecordQueue.end()
               );
            if (findCursor > 0)
            {
                //
                // decrement if found any beyond the first event
                --findCursor;
            }
            logger(L"\tEtwReader::RemoveNextEvent - removed 1 record (Event Id %d), leaving %d events\n", tempRecord.getEventId(), eventRecordQueue.size());
        }
    }
    found_event.swap(tempRecord);
    return bReturn;
}
//////////////////////////////////////////////////////////////////////////////////////////
//
// RemoveEventSet()
//
// Searches the stored runtime ETW events for a std::deque of [in] EtwRecordQuery objects
// and optionally returns the matching records.  Starts the search from the beginning of
// the event queue.
//
// If the find was successful, removes all found events fromt the internal event queue.
// Additionally, if events are removed, the internal cursor is reset.
//
// Arguments:
//      record_query_queue - a reference to a std::deque of EtwRecordQuery objects which 
//                           defines the query parameters of the received events used 
//                           to find a match.
//
//      removed_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
//                            receives a copy of records that match the query objects.
//                            (Optional)
//
//      bRequireInOrder - boolean flag do dictate if the real-time events being searched
//                        should match the EtwRecordQuery objects only if in identical
//                        ordering.
//
//      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
//                      available before returning.
//
// Returns:
//      true - if the events are found and removed in the queue of found events.
//      false - if the events are not found within the entire queue of found events.
//
// On Failure: unable to determine if the event exists in the queue or not.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//
//////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
bool EtwReader<T, L>::RemoveEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, bool bRequireInOrder, unsigned uMilliseconds)
{
    std::deque<ntl::EtwRecord> tempRecords;
    bool bReturn = FindFirstEventSet(record_query_queue, tempRecords, bRequireInOrder, uMilliseconds);
    if (bReturn)
    {
        {
            ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
            assert(record_query_queue.size() == tempRecords.size());
            for ( EtwRecordIterator iterRecord = tempRecords.begin(), iterEnd = tempRecords.end();
                    iterRecord != iterEnd;
                    ++iterRecord)
            {
                //
                // erase-remove idiom to efficiently remove the element
                eventRecordQueue.erase(
                    std::remove(eventRecordQueue.begin(), eventRecordQueue.end(), *iterRecord),
                    eventRecordQueue.end()
                   );
                logger(L"\t\tEtwReader::RemoveEventSet - (Event Id %d) removed\n", iterRecord->getEventId()) ;
            }
            //
            // removing arbitrary events resets the cursor
            findCursor = 0;
            logger(L"\tEtwReader::RemoveEventSet - %d records removed, %d records remain\n", tempRecords.size(), eventRecordQueue.size()) ;
        }
    }
    return bReturn;
}
template <typename T, typename L>
bool EtwReader<T, L>::RemoveEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, bool bRequireInOrder, unsigned uMilliseconds)
{
    std::deque<ntl::EtwRecord> tempRecords;
    bool bReturn = FindFirstEventSet(record_query_queue, tempRecords, bRequireInOrder, uMilliseconds);
    if (bReturn)
    {
        {
            ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
            assert(record_query_queue.size() == tempRecords.size());
            for ( EtwRecordIterator iterRecord = tempRecords.begin(), iterEnd = tempRecords.end();
                    iterRecord != iterEnd;
                    ++iterRecord)
            {
                //
                // erase-remove idiom to efficiently remove the element
                eventRecordQueue.erase(
                    std::remove(eventRecordQueue.begin(), eventRecordQueue.end(), *iterRecord),
                    eventRecordQueue.end()
                   );
                logger(L"\t\tEtwReader::RemoveEventSet - Event Id %d removed\n", iterRecord->getEventId()) ;
            }
            //
            // removing arbitrary events resets the cursor
            //
            findCursor = 0;
            logger(L"\tEtwReader::RemoveEventSet - %d records removed, %d records remain\n", tempRecords.size(), eventRecordQueue.size()) ;
        }
    }
    found_event_queue.swap(tempRecords);
    return bReturn;
}
//////////////////////////////////////////////////////////////////////////////////////////
//
// FindFirstEventSet()
//
// Searches the stored runtime ETW events for a std::deque of [in] EtwRecordQuery objects
// and optionally returns the matching records.  Starts the search from the beginning of
// the event queue.
//
// If the find was successful, updates an internal cursor to one-past the last event found
// to use on subsequent calls to find.
//
// Arguments:
//      record_query_queue - a reference to a std::deque of EtwRecordQuery objects which 
//                           defines the query parameters of the received events used 
//                           to find a match.
//
//      found_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
//                          receives a copy of records that match the query objects.
//                          (Optional)
//
//      bRequireInOrder - boolean flag do dictate if the real-time events being searched
//                        should match the EtwRecordQuery objects only if in identical
//                        ordering.
//
//      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
//                      available before returning.
//
// Returns:
//      true - if the event is found in the queue of found events.
//      false - if the event is not found within the entire queue of found events.
//
// On Failure: unable to determine if the event exists in the queue or not.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//
//////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
bool EtwReader<T, L>::FindFirstEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, bool bRequireInOrder, unsigned uMilliseconds)
{
    //
    // reset the internal cursor to the beginning and do a find
    //
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        findCursor = 0;
    }
    return (FindNextEventSet(record_query_queue, bRequireInOrder, uMilliseconds));
}
template <typename T, typename L>
bool EtwReader<T, L>::FindFirstEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, bool bRequireInOrder, unsigned uMilliseconds)
{
    //
    // reset the internal cursor to the beginning and do a find
    //
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        findCursor = 0;
    }
    csScoped.Leave();
    return (FindNextEventSet(record_query_queue, found_event_queue, bRequireInOrder, uMilliseconds));
}
//////////////////////////////////////////////////////////////////////////////////////////
//
// FindNextEventSet()
//
// Searches the stored runtime ETW events for one or more [in] EtwRecordQuery objects
// and optionally returns the matching records.  Starts the search from where the prior
// call to FindFirstEventSet / FindNextEventSet left off.
//
// If the find was successful, updates an internal cursor to one-past the last event found
// to use on subsequent calls to find.
//
// Arguments:
//      record_query_queue - a reference to a std::deque of EtwRecordQuery objects which 
//                           defines the query parameters of the received events used 
//                           to find a match.
//
//      found_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
//                          receives a copy of records that match the query objects.
//                          (Optional)
//
//      bRequireInOrder - boolean flag do dictate if the real-time events being searched
//                        should match the EtwRecordQuery objects only if in identical
//                        ordering.
//
//      uMilliseconds - time (in milliseconds) to spend waiting for all events to be 
//                      available before returning.
//
// Returns:
//      true - if the event is found in the queue of found events.
//      false - if the event is not found within the entire queue of found events.
//
// On Failure: unable to determine if the event exists in the queue or not.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//
//////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
bool EtwReader<T, L>::FindNextEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, bool bRequireInOrder, unsigned uMilliseconds)
{
    return FindNextEventSetImpl(record_query_queue, NULL, bRequireInOrder, uMilliseconds);
}
template <typename T, typename L>
bool EtwReader<T, L>::FindNextEventSet(const std::deque<ntl::EtwRecordQuery> & record_query_queue, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, bool bRequireInOrder, unsigned uMilliseconds)
{
    return FindNextEventSetImpl(record_query_queue, &(found_event_queue), bRequireInOrder, uMilliseconds);
}
template <typename T, typename L>
bool EtwReader<T, L>::FindNextEventSetImpl(const std::deque<ntl::EtwRecordQuery> & record_query_queue, _Out_opt_ std::deque<ntl::EtwRecord> * pfound_event_queue, bool bRequireInOrder, unsigned uMilliseconds)
{
    //
    // if expected queue is empty, return true
    //
    if (0 == record_query_queue.size())
    {
        if (pfound_event_queue != NULL)
        {
            pfound_event_queue->clear();
        }
        return true;
    }

    // temp queue if the user wants a returned queue
    std::deque<ntl::EtwRecord> temp_Queue;
    // otherwise tracking count for when the out buffer isn't requested
    size_t tCount = 0;
    size_t tFailed = 0;
    // track the last found record, and last failed query
    EtwRecordOffset last_found_record = 0;
    // track the query that wasn't found
    ntl::EtwRecordQuery last_failed_query;
    // monitor time taken
    DWORD startTime = ::GetTickCount();
    bool bReturn = false;
    size_t priorQueueSize = 0;
    while (!bReturn)
    {
        //
        // explicitly flush events from the ETW internal buffers every iteration
        //
        FlushSession();

        //
        // reset the queue and counter each iteration
        //
        temp_Queue.clear();
        tCount = 0;
        {
            ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
            //
            // process the event queue only if new events exist
            // otherwise exit the CS as quickly as possible to reduce contention
            //
            if (eventRecordQueue.size() > priorQueueSize)
            {
                priorQueueSize = eventRecordQueue.size();
                //
                // get the Record and RecordQuery iterators to traverse the queues
                EtwRecordQueryIterator iter_query = record_query_queue.begin();
                EtwRecordQueryIterator  end_query = record_query_queue.end();
                //
                // offset to the findCursor to continue the event search
                EtwRecordIterator  iter_events = eventRecordQueue.begin() + findCursor;
                EtwRecordIterator begin_events = eventRecordQueue.begin();
                EtwRecordIterator   end_events = eventRecordQueue.end();
                //
                // branch if require in-order event records
                if (bRequireInOrder)
                {
                    //
                    // For RequireInOrder
                    //  - iterate until either queue is exhausted
                    //  - or a query didn't find an event
                    //
                    while ((iter_query != end_query) && (iter_events != end_events))
                    {
                        ntl::EtwRecordQueryPredicate predicate(*iter_query);
                        iter_events = std::find_if(
                            iter_events,
                            end_events,
                            predicate
                           );
                        if (iter_events != end_events)
                        {
                            //
                            // track the last-found event for the cursor
                            last_found_record = (iter_events - begin_events) + 1;
                            //
                            // Store the event object for what will be returned in the [out] queue if requested
                            if (pfound_event_queue != NULL)
                            {
                                temp_Queue.push_back(*iter_events);
                            }
                            //
                            // count the event as another one found
                            ++tCount;
                            //
                            // move to the next event record and record query
                            ++iter_events;
                            ++iter_query;
                        }
                        else
                        {
                            ++tFailed;
                            last_failed_query = *iter_query;
                        }
                    }
                }
                else // !bRequireInOrder
                {
                    //
                    // Since the original eventRecordQueue std::deque isn't being modified, we can track all found events
                    //   by storing the iterator offset (instance from begin()).  Need to do this instead of just 
                    //   comparing the events we found since the caller could have asked for the same event multiple times.
                    //   (multiple identical expected event filters)
                    //
                    std::deque< EtwRecordOffset > previouslyFoundRecords;
                    bool bContinueSearch = true;
                    for ( ; bContinueSearch && (iter_query != end_query); ++iter_query)
                    {
                        //
                        // look for each expected-event-filter across the *entire* event queue
                        // (not requiring in-order, so could be anywhere in the queue)
                        // - verify not matching the same event twice with different filters though
                        //
                        ntl::EtwRecordQueryPredicate predicate(*iter_query);
                        EtwRecordIterator findIterate = iter_events;
                        while (findIterate != end_events)
                        {
                            findIterate = std::find_if(
                                findIterate,
                                end_events,
                                predicate
                               );
                            if (findIterate != end_events)
                            {
                                //
                                // Found a matching event in eventRecordQueue.
                                // Need to see if we've seen this specific on before in our difference_type queue.
                                //
                                EtwRecordOffset foundDiff = findIterate - begin_events;
                                std::deque< EtwRecordOffset >::const_iterator repeatRecord = std::find(
                                    previouslyFoundRecords.begin(), 
                                    previouslyFoundRecords.end(), 
                                    foundDiff
                                   );
                                if (repeatRecord == previouslyFoundRecords.end())
                                {
                                    //
                                    // This event wasn't previously found
                                    //
                                    // track the last-found event for the cursor
                                    last_found_record = (findIterate - begin_events) + 1;
                                    //
                                    // Store the event object for what will be returned in the [out] queue if requested
                                    if (pfound_event_queue != NULL)
                                    {
                                        temp_Queue.push_back(*findIterate);
                                    }
                                    //
                                    // count the event as another one found
                                    ++tCount;
                                    //
                                    // Store the iterator offset in previouslyFoundRecords as to never store this same event again
                                    previouslyFoundRecords.push_back(foundDiff);
                                    //
                                    // Explicitly break out of the for() loop here -> don't continue searching the message queue
                                    // but do continue to iterate through the queue of event queries
                                    bContinueSearch = true;
                                    break;
                                }
                                else
                                {
                                    //
                                    // event was in the list of events we've seen
                                    // - move to the next event and continue the search
                                    //
                                    ++findIterate;
                                }
                            }
                            else
                            {
                                //
                                // Searched the entire message queue and didn't match the query
                                //
                                // Explicitly break out of the for() loop here -> don't continue searching the message queue
                                // - and don't contine to iterate through the queue of event queries (we've already failed)
                                ++tFailed;
                                last_failed_query = *iter_query;
                                bContinueSearch = false;
                                break;
                            }
                        } // while loop iterating event records
                    } // for-loop iterating event queries
                } // if-else branching on bRequireInOrder
            } // if-block branching if the event queue has any records
        }
        //
        // determine if found all expected events
        //
        if (pfound_event_queue != NULL)
        {
            assert(temp_Queue.size() == tCount);
        }
            
        bReturn = (tCount == record_query_queue.size());
        if (!bReturn)
        {
            //
            // if time expired break out now, else sleep
            DWORD currentTime = ::GetTickCount();
            DWORD elapsedTime = (currentTime < startTime) ? // did tick count rollover?
                currentTime + (MAXDWORD - startTime) : // rolled over, so calculate the difference
                currentTime - startTime;  // didn't roll over
            if (elapsedTime > uMilliseconds)
            {
                break;
            }
            else
            {
                ::Sleep(SLEEP_TIME);
            }
        }
    } // while (!bReturn)

    if (bReturn)
    {
        //
        // If found all expected events, update the findCursor to one past the last-found event
        // - and swap and return the event queue
        //
        logger(L"\tEtwReader::FindEventSet Found %d events in a queue with %d records\n", 
            tCount, 
            eventRecordQueue.size()
           );
        findCursor = last_found_record + 1;
        if (pfound_event_queue != NULL)
        {
            pfound_event_queue->swap(temp_Queue);
        }
    }
    else if (0 == tFailed)
    {
        logger(L"\tEtwReader::FindEventSet - failed all record queries\n");
    }
    else
    {
        try
        {
            std::wstring wsText;
            last_failed_query.writeQuery(wsText);
            logger(L"\tEtwReader::FindEventSet failed to find the specific record query:\n%s\n",
                wsText.c_str()
               );
        }
        catch (const std::exception&)
        {
            // don't fail the call if just can't allocate for the logger
        }
    }
    return bReturn;
}
    
//////////////////////////////////////////////////////////////////////////////////////////
//
// FindAllMatchingEvents()
//
// Searches the stored runtime ETW events for all instances of the [in] EtwRecordQuery 
// object, and optionally returns a queue of the matching records.
//
// Arguments:
//      record_query - a reference to a EtwRecordQuery instance which defines the
//                     query parameters used to find a match.
//
//      found_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
//                          receives a copy of records that match the query object.
//
// Returns:
//      true - if at least one matching event is found in the queue of found events.
//      false - if no matching events are found within the entire queue of found events.
//
// On Failure: unable to determine if the event exists in the queue or not.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//
//////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
bool EtwReader<T, L>::FindAllMatchingEvents(const ntl::EtwRecordQuery & record_query, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, unsigned uMilliseconds)
{
    DWORD startTime = ::GetTickCount();
    //
    // temp queue - reuse the callers buffer
    std::deque<ntl::EtwRecord> temp_Queue;
    temp_Queue.swap(found_event_queue);
    temp_Queue.clear();
    bool bReturn = false;
    while (!bReturn)
    {
        //
        // explicitly flush events from the ETW internal buffers every iteration
        //
        FlushSession();

        {
            ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
            //
            // look for each expected-event-filter across the *entire* event queue
            // (not requiring in-order, so could be anywhere in the queue)
            // - verify not matching the same event twice with different filters though
            //
            ntl::EtwRecordQueryPredicate predicate(record_query);
            EtwRecordIterator iter_events = eventRecordQueue.begin();
            EtwRecordIterator  end_events = eventRecordQueue.end();
            while (iter_events != end_events)
            {
                iter_events = std::find_if(
                    iter_events,
                    end_events,
                    predicate
                   );
                if (iter_events != end_events)
                {
                    temp_Queue.push_back(*iter_events);
                    ++iter_events;
                }
            }
        }
        bReturn = temp_Queue.size() > 0;
        if (!bReturn)
        {
            //
            // if time expired break out now, else sleep
            DWORD currentTime = ::GetTickCount();
            DWORD elapsedTime = (currentTime < startTime) ? // did tick count rollover?
                currentTime + (MAXDWORD - startTime) : // rolled over, so calculate the difference
                currentTime - startTime;  // didn't roll over
            if (elapsedTime > uMilliseconds)
            {
                break;
            }
            else
            {
                ::Sleep(SLEEP_TIME);
            }
        }
    }
    //
    // swap and return
    //
    if (bReturn)
    {
        found_event_queue.swap(temp_Queue);
    }
    else
    {
        try
        {
            std::wstring wsText;
            record_query.writeQuery(wsText);
            logger(L"\tEtwReader::FindAllMatchingEvents failed to find the query: %s\n",
                wsText.c_str()
               );
        }
        catch (const std::exception&)
        {
            // don't fail the call if just can't allocate for the logger
        }
    }
    return bReturn;
}


//////////////////////////////////////////////////////////////////////////////////////////
//
// RemoveAllMatchingEvents()
//
// Searches the stored runtime ETW events for all instances of the [in] EtwRecordQuery 
// object, removes them, and optionally returns a queue of the matching records.
//
// Arguments:
//      record_query - a reference to a EtwRecordQuery instance which defines the
//                     query parameters used to find a match.
//
//      found_event_queue - a reference to a std::deque of EtwRecordQuery objects which 
//                          receives a copy of records that match the query object.
//
// Returns:
//      true - if at least one matching event is found in the queue of found events.
//      false - if no matching events are found within the entire queue of found events.
//
// On Failure: unable to determine if the event exists in the queue or not.
//      An exception derived from std::exception will be thrown
//      - low resource conditions will throw std::bad_alloc
//
//////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
bool EtwReader<T, L>::RemoveAllMatchingEvents(const ntl::EtwRecordQuery & record_query, _Out_ std::deque<ntl::EtwRecord> & found_event_queue, unsigned uMilliseconds)
{
    std::deque<ntl::EtwRecord> tempRecords;
    bool bReturn = FindAllMatchingEvents(record_query, tempRecords, uMilliseconds);
    if (bReturn)
    {
        {
            ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
            for ( EtwRecordIterator iterRecord = tempRecords.begin(), iterEnd = tempRecords.end();
                    iterRecord != iterEnd;
                    ++iterRecord)
            {
                //
                // erase-remove idiom to efficiently remove the element
                eventRecordQueue.erase(
                    std::remove(eventRecordQueue.begin(), eventRecordQueue.end(), *iterRecord),
                    eventRecordQueue.end()
                   );
            }
            //
            // removing arbitrary events resets the cursor
            findCursor = 0;
            logger(L"\tEtwReader::RemoveAllMatchingEvents - %d records removed, %d records remain\n", tempRecords.size(), eventRecordQueue.size()) ;
        }
    }
        
    found_event_queue.swap(tempRecords);
    return bReturn;
}
    
////////////////////////////////////////////////////////////////////////////////
//
// CountEvents()
//
// Arguments: None
//
// Returns the current number of events queued through the real-time event policy.
// Can be called while events are still being processed real-time.
//
// Cannot Fail - Will not throw.
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
size_t EtwReader<T, L>::CountEvents() NOEXCEPT
{
    //
    // CS object enters the CS and leaves when exits scope
    //
    size_t tReturn = 0;
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        tReturn = eventRecordQueue.size();
    }
    return tReturn;
}


////////////////////////////////////////////////////////////////////////////////
//
// FlushEvents()
//
// Arguments: None
//
// Cannot Fail - Will not throw.
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
size_t EtwReader<T, L>::FlushEvents() NOEXCEPT
{
    //
    // CS object enters the CS and leaves when exits scope
    //
    size_t tReturn = 0;
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        tReturn = eventRecordQueue.size();
        eventRecordQueue.clear();
    }
    return tReturn;
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
// FlushEvents()
//
// Arguments:
//      foundEvents - an reference to an [out] std::deque to be populated with all events
//                    found from the real-time even policy. (Optional)
//
// Cannot Fail - Will not throw.
//      - a copy is not made of the events - they are swapped into the provided std::deque,
//        which cannot fail (no copying required, thus no OOM condition possible).
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
size_t EtwReader<T, L>::FlushEvents(_Out_ std::deque<ntl::EtwRecord>& out_queue) NOEXCEPT
{
    //
    // CS object enters the CS and leaves when exits scope
    //
    size_t tReturn = 0;
    {
        ntl::AutoReleaseCriticalSection csScoped(&csRecordQueue);
        tReturn = eventRecordQueue.size();
        eventRecordQueue.swap(out_queue);
        eventRecordQueue.clear();
    }
    return tReturn;
}
    
    
    
////////////////////////////////////////////////////////////////////////////////
//
// ProcessTraceThread()
//
// EtwReader objects spin up a worker thread executing this function
//      to block on the call to ProcessTrace.  ProcessTrace is a blocking call,
//      and thus needs its own dedicated thread to block on while receiving events.
//
// Arguments:
//      lpParameter - a ptr to the TRACEHANDLE that was allocated in the parent
//                    EtwReader object to pass to ProcessTrace()
//
// Returns: the return from ProcessTrace
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
DWORD WINAPI EtwReader<T, L>::ProcessTraceThread(LPVOID lpParameter)
{
    //
    // Must call ProcessTrace to start the events going to the callback
    //
    ULONG ulReturn = ::ProcessTrace(
        reinterpret_cast<TRACEHANDLE*>(lpParameter),
        1,
        NULL,
        NULL
   );
    return ulReturn;
}
    
    
////////////////////////////////////////////////////////////////////////////////
//
// EventRecordCallback()
//
// EtwReader passes this function pointer to the ETW Tracing APIs to
//      be called in an ETW callback thread whenever the ETW buffers are flushed
//      and events passed back to the callback.
//
// This callback function takes the EVENT_RECORD pointer, passes it through the
//      policy template (::Filter()), and if the filter returns true, creates
//      a EtwRecord object to deep-copy the EVENT_RECORD, and adds the
//      EtwRecord to the parent object event queue (::AddEventRecord())
//
// Arguments:
//      lpParameter - a ptr to the parent EtwReader object that passed
//                    this function ptr to ETW.
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
VOID WINAPI EtwReader<T, L>::EventRecordCallback(PEVENT_RECORD pEventRecord)
{
    EtwReader* peventReader = 
        reinterpret_cast<EtwReader*>(pEventRecord->UserContext);
    
    try
    {
        //
        // When opening a saved session from an ETL file, the first event record
        // contains diagnostic information about the contents of the trace - the
        // most important (for us) field being the number of buffers written. By
        // saving this value, we can consume it later on inside BufferCallback to
        // force ProcessTrace() to return when the entire contents of the session
        // have been read.
        //
        bool process = true;
        if (peventReader->openSavedSession && !peventReader->initNumBuffers)
        {
            ntl::EtwRecord eventMessage(pEventRecord);
            std::wstring task;
            if (eventMessage.queryTaskName(task) && task == L"EventTrace")
            {
                process = false;
                ntl::EtwRecord::PropertyPair pair;
                if (eventMessage.queryEventProperty(L"BuffersWritten", pair))
                {
                    peventReader->initNumBuffers = true;
                    peventReader->numBuffers = *(int*)pair.first.data();
                }
            }
        }

        if (process && peventReader->Filter(pEventRecord))
        {
            //
            // create a temp ntEvtTrace object with the passed-in record
            //
            ntl::EtwRecord eventMessage(pEventRecord);
            peventReader->AddEventRecord(eventMessage);
        }
    }
    catch(const std::exception&)
    {
        //
        // the above could throw std::exception objects (e.g. std::bad_alloc)
        // - or Exception objects (which derive from std::exception)
        //
    }
    
    return;
}


////////////////////////////////////////////////////////////////////////////////
//
// BufferCallback()
//
// EtwReader passes this function pointer to the ETW Tracing APIs to
//      be called in an ETW callback thread whenever the events for each buffer are delivered.
//
// Arguments:
//      Buffer - a ptr to an EVENT_TRACE_LOGFILE structure that contains information about the buffer.
//
// Returns: FALSE to terminate the ProcessTrace() function, TRUE otherwise
//
////////////////////////////////////////////////////////////////////////////////
template <typename T, typename L>
ULONG WINAPI EtwReader<T, L>::BufferCallback(PEVENT_TRACE_LOGFILE Buffer)
{
    EtwReader* peventReader =
        reinterpret_cast<EtwReader*>(Buffer->Context);
    return (Buffer->BuffersRead != peventReader->numBuffers);
}

} // namespace
