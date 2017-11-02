// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

#include <iterator>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <tuple>
#include <algorithm>

#include <Windows.h>

#include <ntlThreadpoolTimer.hpp>
#include <ntlException.hpp>
#include <ntlWmiInitialize.hpp>
#include <ntlScopeGuard.hpp>
#include <ntlString.hpp>
#include <ntlLocks.hpp>


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// Concepts for this class:
/// - WMI Classes expose performance counters through hi-performance WMI interfaces
/// - WmiPerformanceCounter exposes one counter within one WMI performance counter class
/// - Every performance counter object contains a 'Name' key field, uniquely identifying a 'set' of data points for that counter
/// - Counters are 'snapped' every one second, with the timeslot tracked with the data
/// 
/// WmiPerformanceCounter is exposed to the user through factory functions defined per-counter class (ctShared<class>PerfCounter)
/// - the factory functions takes in the counter name that the user wants to capture data for
/// - the factory function has a template type matching the data type of the counter data for that counter name
/// 
/// Internally, the factory function instantiates a WmiPerformanceCounterImpl:
/// - has 3 template arguments:
/// 1. The IWbem* interface used to enumerate instances of this performance class (either IWbemHiPerfEnum or IWbemClassObject)
/// 2. The IWbem* interface used to access data in perf instances of this performance class (either IWbemObjectAccess or IWbemClassObject)
/// 3. The data type of the values for the counter name being recorded
/// - has 2 function arguments
/// 1. The string value of the WMI class to be used
/// 2. The string value of the counter name to be recorded
/// 
/// Methods exposed publicly off of WmiPerformanceCounter:
/// - add_filter(): allows the caller to only capture instances which match the parameter/value combination for that object
/// - reference_range() : takes an Instance Name by which to return values
/// -- returns begin/end iterators to reference the data
/// 
/// WmiPerformanceCounter populates data by invoking a pure virtual function (update_counter_data) every one second.
/// - update_counter_data takes a boolean parameter: true will invoke the virtual function to update the data, false will clear the data.
/// That pure virtual function (WmiPerformanceCounterImpl::update_counter_data) refreshes its counter (through its template accessor interface)
/// - then iterates through each instance recorded for that counter and invokes add_instance() in the base class.
/// 
/// add_instance() takes a WMI* of teh Accessor type: if that instance wasn't explicitly filtered out (through an instance_filter object),
/// - it looks to see if this is a new instance or if we have already been tracking that instance
/// - if new, we create a new WmiPerformanceCounterData object and add it to our counter_data
/// - if not new, we just add this object to the counter_data object that we already created
/// 
/// There are 2 possible sets of WMI interfaces to access and enumerate performance data, these are defined in WmiPerformanceDataAccessor
/// - this is instantiated in WmiPerformanceCounterImpl as it knows the Access and Enum template types
/// 
/// WmiPerformanceCounterData encapsulates the data points for one instance of one counter.
/// - exposes match() taking a string to check if it matches the instance it contains
/// - exposes add() taking both types of Access objects + a ULONGLONG time parameter to retrieve the data and add it to the internal map
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ntl {
    enum class WmiPerformanceCollectionType
    {
        Detailed,
        MeanOnly,
        FirstLast
    };

    namespace {
        ////////////////////////////////////////////////////////////////////////////////////////////////////
        ///
        /// Function to return the performance data of the specified property from the input instance
        ///
        ////////////////////////////////////////////////////////////////////////////////////////////////////
        inline
        ntl::ComVariant ReadIWbemObjectAccess(_In_ IWbemObjectAccess* _instance, _In_ LPCWSTR _counter_name)
        {
            LONG property_handle;
            CIMTYPE property_type;
            HRESULT hr = _instance->GetPropertyHandle(_counter_name, &property_type, &property_handle);
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"IWbemObjectAccess::GetPropertyHandle", L"WmiPerformance::ReadIWbemObjectAccess", false);
            }

            ntl::ComVariant current_value;
            switch (property_type) {
                case CIM_SINT32:
                case CIM_UINT32: {
                    ULONG value;
                    hr = _instance->ReadDWORD(property_handle, &value);
                    if (FAILED(hr)) {
                        throw ntl::Exception(hr, L"IWbemObjectAccess::ReadDWORD", L"WmiPerformance::ReadIWbemObjectAccess", false);
                    }
                    current_value.assign<VT_UI4>(value);
                    break;
                }

                case CIM_SINT64:
                case CIM_UINT64: {
                    ULONGLONG value;
                    hr = _instance->ReadQWORD(property_handle, &value);
                    if (FAILED(hr)) {
                        throw ntl::Exception(hr, L"IWbemObjectAccess::ReadQWORD", L"WmiPerformance::ReadIWbemObjectAccess", false);
                    }
                    current_value.assign<VT_UI8>(value);
                    break;
                }

                case CIM_STRING: {
                    ntl::ComBstr value;
                    value.resize(64);
                    long value_size = static_cast<long>(value.size() * sizeof(WCHAR));
                    long returned_size;
                    hr = _instance->ReadPropertyValue(property_handle, value_size, &returned_size, reinterpret_cast<BYTE*>(value.get()));
                    if (WBEM_E_BUFFER_TOO_SMALL == hr) {
                        value_size = returned_size;
                        value.resize(value_size);
                        hr = _instance->ReadPropertyValue(property_handle, value_size, &returned_size, reinterpret_cast<BYTE*>(value.get()));
                    }
                    if (FAILED(hr)) {
                        throw ntl::Exception(hr, L"IWbemObjectAccess::ReadPropertyValue", L"WmiPerformance::ReadIWbemObjectAccess", false);
                    }
                    current_value.assign<VT_BSTR>(value.get());
                    break;
                }

                default:
                    throw ntl::Exception(
                        ERROR_INVALID_DATA,
                        String::format_string(
                            L"WmiPerformance only supports data of type INT32, INT64, and BSTR: counter %s is of type %u",
                            _counter_name, property_type).c_str(),
                        L"WmiPerformance::ReadIWbemObjectAccess",
                        true);
            }

            return current_value;
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        ///
        /// template class WmiPerformanceDataAccessor
        ///
        /// Refreshes performance data for the target specified based off the classname
        /// - and the template types specified [the below are the only types supported]:
        ///
        /// Note: caller *MUST* provide thread safety
        ///       this class is not providing locking at this level
        ///
        /// Note: callers *MUST* guarantee connections with the WMI service stay connected
        ///       for the lifetime of this object [e.g. guarnated WmiService is instanitated]
        ///
        /// Note: callers *MUST* guarantee that COM is CoInitialized on this thread before calling
        ///
        /// Note: the WmiPerformance class *will* retain WMI service instance
        ///       it's recommended to guarantee it stays alive
        ///
        /// Template typename options:
        ///
        /// typename TEnum == IWbemHiPerfEnum
        /// - encapsulates the processing of IWbemHiPerfEnum instances of type _classname
        ///
        /// typename TEnum == IWbemClassObject
        /// - encapsulates the processing of a single refreshable IWbemClassObject of type _classname
        ///
        /// typename TAccess == IWbemObjectAccess
        /// - begin/end return an iterator to a vector<IWbemObjectAccess> of refreshed perf data
        /// - Note: could be N number of instances
        ///
        /// typename TAccess == IWbemClassObject
        /// - begin/end return an iterator to a vector<IWbemClassObject> of refreshed perf data
        /// - Note: will only ever be a single instance
        ///
        ////////////////////////////////////////////////////////////////////////////////////////////////////
        template <typename TEnum, typename TAccess>
        class WmiPerformanceDataAccessor {
        public:
            typedef typename std::vector<TAccess*>::const_iterator access_iterator;

            WmiPerformanceDataAccessor(_In_ ntl::ComPtr<IWbemConfigureRefresher> _config, _In_ LPCWSTR _classname);

            ~WmiPerformanceDataAccessor() NOEXCEPT
            {
                clear();
            }

            ///
            /// refreshes internal data with the latest performance data
            ///
            void refresh();

            access_iterator begin() const NOEXCEPT
            {
                return accessor_objects.begin();
            }
            access_iterator end() const NOEXCEPT
            {
                return accessor_objects.end();
            }

            // non-copyable
            WmiPerformanceDataAccessor(const WmiPerformanceDataAccessor&) = delete;
            WmiPerformanceDataAccessor& operator=(const WmiPerformanceDataAccessor&) = delete;

            // movable
            WmiPerformanceDataAccessor(WmiPerformanceDataAccessor&& rhs) :
                enumeration_object(std::move(rhs.enumeration_object)),
                accessor_objects(std::move(rhs.accessor_objects)),
                current_iterator(std::move(rhs.current_iterator))
            {
                // since accessor_objects is storing raw pointers, manually clear out the rhs object
                // so they won't be double-deleted
                rhs.accessor_objects.clear();
                rhs.current_iterator = rhs.accessor_objects.end();
            }

        private:
            // members
            ntl::ComPtr<TEnum> enumeration_object;
            // TAccess pointers are returned through enumeration_object::GetObjects, reusing the same vector for each refresh call
            std::vector<TAccess*> accessor_objects;
            access_iterator current_iterator;

            void clear() NOEXCEPT;
        };

        inline
        WmiPerformanceDataAccessor<IWbemHiPerfEnum, IWbemObjectAccess>::WmiPerformanceDataAccessor(_In_ ntl::ComPtr<IWbemConfigureRefresher> _config, _In_ LPCWSTR _classname)
        : enumeration_object(),
          accessor_objects(),
          current_iterator(accessor_objects.end())
        {
            // must load COM and WMI locally, though both are still required globally
            ntl::ComInitialize com;
            WmiService wmi(L"root\\cimv2");

            LONG lid;
            HRESULT hr = _config->AddEnum(wmi.get(), _classname, 0, NULL, enumeration_object.get_addr_of(), &lid);
            if (FAILED(hr)) {
                throw ntl::Exception(
                    hr,
                    L"IWbemConfigureRefresher::AddEnum",
                    L"WmiPerformanceDataAccessor<IWbemHiPerfEnum, IWbemObjectAccess>::WmiPerformanceDataAccessor",
                    false);
            }
        }

        inline
        WmiPerformanceDataAccessor<IWbemClassObject, IWbemClassObject>::WmiPerformanceDataAccessor(_In_ ntl::ComPtr<IWbemConfigureRefresher> _config, _In_ LPCWSTR _classname)
        : enumeration_object(),
          accessor_objects(),
          current_iterator(accessor_objects.end())
        {
            // must load COM and WMI locally, though both are still required globally
            ntl::ComInitialize com;
            WmiService wmi(L"root\\cimv2");

            WmiEnumerate enum_instances(wmi);
            enum_instances.query(String::format_string(L"SELECT * FROM %s", _classname).c_str());
            if (enum_instances.begin() == enum_instances.end()) {
                throw ntl::Exception(
                    ERROR_NOT_FOUND,
                    String::format_string(
                        L"Failed to refresh a static instances of the WMI class %s",
                        _classname).c_str(),
                    L"WmiPerformanceDataAccessor",
                    true);
            }

            auto instance = *enum_instances.begin();
            LONG lid;
            HRESULT hr = _config->AddObjectByTemplate(
                wmi.get(),
                instance.get_instance().get(),
                0,
                NULL,
                enumeration_object.get_addr_of(),
                &lid);
            if (FAILED(hr)) {
                throw ntl::Exception(
                    hr,
                    L"IWbemConfigureRefresher::AddObjectByTemplate",
                    L"WmiPerformanceDataAccessor<IWbemClassObject, IWbemClassObject>::WmiPerformanceDataAccessor",
                    false);
            }

            // setting the raw pointer in the access vector to behave with the iterator
            accessor_objects.push_back(enumeration_object.get());
        }

        template <>
        inline void WmiPerformanceDataAccessor<IWbemHiPerfEnum, IWbemObjectAccess>::refresh()
        {
            clear();

            ULONG objects_returned = 0;
            HRESULT hr = enumeration_object->GetObjects(
                0,
                static_cast<ULONG>(accessor_objects.size()),
                (0 == accessor_objects.size()) ? nullptr : &accessor_objects[0],
                &objects_returned);

            if (WBEM_E_BUFFER_TOO_SMALL == hr) {
                accessor_objects.resize(objects_returned);
                hr = enumeration_object->GetObjects(
                    0,
                    static_cast<ULONG>(accessor_objects.size()),
                    &accessor_objects[0],
                    &objects_returned);
            }
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"IWbemObjectAccess::GetObjects", L"WmiPerformanceDataAccessor<IWbemHiPerfEnum, IWbemObjectAccess>::refresh", false);
            }

            accessor_objects.resize(objects_returned);
            current_iterator = accessor_objects.begin();
        }

        template <>
        inline void WmiPerformanceDataAccessor<IWbemClassObject, IWbemClassObject>::refresh()
        {
            // the underlying IWbemClassObject is already refreshed
            // accessor_objects will only ever have a singe instance
            FatalCondition(
                accessor_objects.size() != 1,
                L"WmiPerformanceDataAccessor<IWbemClassObject, IWbemClassObject>: for IWbemClassObject performance classes there can only ever have the single instance being tracked - instead has %Iu",
                accessor_objects.size());

            current_iterator = accessor_objects.begin();
        }

        template <>
        inline void WmiPerformanceDataAccessor<IWbemHiPerfEnum, IWbemObjectAccess>::clear() NOEXCEPT
        {
            for (IWbemObjectAccess* _object : accessor_objects) {
                _object->Release();
            }
            accessor_objects.clear();
            current_iterator = accessor_objects.end();
        }

        template <>
        inline void WmiPerformanceDataAccessor<IWbemClassObject, IWbemClassObject>::clear() NOEXCEPT
        {
            current_iterator = accessor_objects.end();
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        ///
        /// Stucture to track the performance data for each property desired for the instance being tracked
        ///
        /// typename T : the data type of the counter to be stored
        ///
        /// Note: callers *MUST* guarantee connections with the WMI service stay connected
        ///       for the lifetime of this object [e.g. guarnated WmiService is instanitated]
        /// Note: callers *MUST* guarantee that COM is CoInitialized on this thread before calling
        /// Note: the WmiPerformance class *will* retain WMI service instance
        ///       it's recommended to guarantee it stays alive
        ///
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        template <typename T>
        class WmiPeformanceCounterData {
        private:
            mutable CRITICAL_SECTION guard_data;
            const WmiPerformanceCollectionType collection_type;
            const std::wstring instance_name;
            const std::wstring counter_name;
            std::vector<T> counter_data;
            ULONGLONG counter_sum = 0;

            void add_data(const T& instance_data)
            {
                ntl::AutoReleaseCriticalSection auto_guard(&guard_data);
                switch (collection_type) {
                    case WmiPerformanceCollectionType::Detailed:
                        counter_data.push_back(instance_data);
                        break;

                    case WmiPerformanceCollectionType::MeanOnly:
                        // vector is formatted as:
                        // [0] == count
                        // [1] == min
                        // [2] == max
                        // [3] == mean
                        if (counter_data.empty()) {
                            counter_data.push_back(1);
                            counter_data.push_back(instance_data);
                            counter_data.push_back(instance_data);
                            counter_data.push_back(0);
                        } else {
                            ++counter_data[0];

                            if (instance_data < counter_data[1]) {
                                counter_data[1] = instance_data;
                            }
                            if (instance_data > counter_data[2]) {
                                counter_data[2] = instance_data;
                            }
                        }

                        counter_sum += instance_data;
                        break;

                    case WmiPerformanceCollectionType::FirstLast:
                        // the first data point write both min and max
                        // [0] == count
                        // [1] == first
                        // [2] == last
                        if (counter_data.empty()) {
                            counter_data.push_back(1);
                            counter_data.push_back(instance_data);
                            counter_data.push_back(instance_data);
                        } else {
                            ++counter_data[0];
                            counter_data[2] = instance_data;
                        }
                        break;

                    default:
                        AlwaysFatalCondition(
                            L"Unknown WmiPerformanceCollectionType (%u)",
                            collection_type);
                }
            }

            typename std::vector<T>::const_iterator access_begin()
            {
                ntl::AutoReleaseCriticalSection auto_guard(&guard_data);
                // when accessing data, calculate the mean
                if (WmiPerformanceCollectionType::MeanOnly == collection_type) {
                    counter_data[3] = static_cast<T>(counter_sum / counter_data[0]);
                }
                return counter_data.cbegin();
            }

            typename std::vector<T>::const_iterator access_end() const
            {
                ntl::AutoReleaseCriticalSection auto_guard(&guard_data);
                return counter_data.cend();
            }

        public:
            WmiPeformanceCounterData(
                const WmiPerformanceCollectionType _collection_type,
                _In_ IWbemObjectAccess* _instance,
                _In_ LPCWSTR _counter)
            : collection_type(_collection_type),
              instance_name(ReadIWbemObjectAccess(_instance, L"Name")->bstrVal),
              counter_name(_counter)
            {
                if (!::InitializeCriticalSectionEx(&guard_data, 4000, 0)) {
                    auto gle = ::GetLastError();
                    AlwaysFatalCondition(
                        String::format_string(
                            L"InitializeCriticalSectionEx failed with error %ul",
                            gle).c_str());
                }
            }
            WmiPeformanceCounterData(
                const WmiPerformanceCollectionType _collection_type,
                _In_ IWbemClassObject* _instance,
                _In_ LPCWSTR _counter)
            : collection_type(_collection_type),
              counter_name(_counter)
            {
                if (!::InitializeCriticalSectionEx(&guard_data, 4000, 0)) {
                    auto gle = ::GetLastError();
                    AlwaysFatalCondition(
                        String::format_string(
                            L"InitializeCriticalSectionEx failed with error %ul",
                            gle).c_str());
                }

                ntl::ComVariant value;
                HRESULT hr = _instance->Get(L"Name", 0, value.get(), nullptr, nullptr);
                if (FAILED(hr)) {
                    throw ntl::Exception(hr, L"IWbemClassObject::Get(Name)", L"WmiPerformanceCounterData", false);
                }
                // Name is expected to be NULL in this case
                // - since IWbemClassObject is expected to be a single instance
                if (!value.is_null()) {
                    throw ntl::Exception(
                        ERROR_INVALID_DATA,
                        String::format_string(
                            L"WmiPeformanceCounterData was given an IWbemClassObject to track that had a non-null 'Name' key field ['%s']. Expected to be a NULL key field as to only support single-instances",
                            value->bstrVal).c_str(),
                        L"WmiPeformanceCounterData",
                        true);
                }
            }
            ~WmiPeformanceCounterData() NOEXCEPT
            {
                ::DeleteCriticalSection(&guard_data);
            }

            /// _instance_name == nullptr means match everything
            /// - allows for the caller to not have to pass Name filters multiple times
            bool match(_In_opt_ LPCWSTR _instance_name) NOEXCEPT
            {
                if (nullptr == _instance_name) {
                    return true;

                } else if (instance_name.empty()) {
                    return (nullptr == _instance_name);

                } else {
                    return String::iordinal_equals(instance_name, _instance_name);
                }
            }

            void add(_In_ IWbemObjectAccess* _instance)
            {
                T instance_data;
                ReadIWbemObjectAccess(_instance, counter_name.c_str()).retrieve(&instance_data);
                add_data(instance_data);
            }
            void add(_In_ IWbemClassObject* _instance)
            {
                ntl::ComVariant value;
                HRESULT hr = _instance->Get(counter_name.c_str(), 0, value.get(), nullptr, nullptr);
                if (FAILED(hr)) {
                    throw ntl::Exception(
                        hr,
                        String::format_string(
                            L"IWbemClassObject::Get(%s)",
                            counter_name.c_str()).c_str(),
                        L"WmiPeformanceCounterData<T>::add",
                        true);
                }

                T instance_data;
                add_data(value.retrieve(&instance_data));
            }

            typename std::vector<T>::const_iterator begin()
            {
                ntl::AutoReleaseCriticalSection auto_guard(&guard_data);
                return access_begin();
            }
            typename std::vector<T>::const_iterator end()
            {
                ntl::AutoReleaseCriticalSection auto_guard(&guard_data);
                return access_end();
            }

            size_t count()
            {
                ntl::AutoReleaseCriticalSection auto_guard(&guard_data);
                return access_end() - access_begin();
            }

            void clear()
            {
                ntl::AutoReleaseCriticalSection auto_guard(&guard_data);
                counter_data.clear();
                counter_sum = 0;
            }

            // non-copyable
            WmiPeformanceCounterData(const WmiPeformanceCounterData&) = delete;
            WmiPeformanceCounterData& operator= (const WmiPeformanceCounterData&) = delete;

            // not movable
            WmiPeformanceCounterData(WmiPeformanceCounterData&&) = delete;
            WmiPeformanceCounterData& operator=(WmiPeformanceCounterData&&) = delete;
        };

        ///
        /// WMI passes around 64-bit integers as BSTR's, so must specialize reading those values to do the proper conversion
        ///
        template <>
        inline void WmiPeformanceCounterData<ULONGLONG>::add(_In_ IWbemClassObject* _instance)
        {
            ntl::ComVariant value;
            HRESULT hr = _instance->Get(counter_name.c_str(), 0, value.get(), nullptr, nullptr);
            if (FAILED(hr)) {
                throw ntl::Exception(
                    hr,
                    String::format_string(
                        L"IWbemClassObject::Get(%s)",
                        counter_name.c_str()).c_str(),
                    L"WmiPeformanceCounterData<ULONGLONG>::add",
                    true);
            }
            if (value->vt != VT_BSTR) {
                throw ntl::Exception(
                    value->vt,
                    L"Expected a BSTR type to read a ULONGLONG from the IWbemClassObject - unexpected variant type",
                    L"WmiPeformanceCounterData<ULONGLONG>::add",
                    false);
            }

            add_data(::_wcstoui64(value->bstrVal, NULL, 10));
        }

        template <>
        inline void WmiPeformanceCounterData<LONGLONG>::add(_In_ IWbemClassObject* _instance)
        {
            ntl::ComVariant value;
            HRESULT hr = _instance->Get(counter_name.c_str(), 0, value.get(), nullptr, nullptr);
            if (FAILED(hr)) {
                throw ntl::Exception(
                    hr,
                    String::format_string(
                        L"IWbemClassObject::Get(%s)",
                        counter_name.c_str()).c_str(),
                    L"WmiPeformanceCounterData<LONGLONG>::add",
                    true);
            }
            if (value->vt != VT_BSTR) {
                throw ntl::Exception(
                    value->vt,
                    L"Expected a BSTR type to read a ULONGLONG from the IWbemClassObject - unexpected variant type",
                    L"WmiPeformanceCounterData<LONGLONG>::add",
                    false);
            }

            add_data(::_wcstoi64(value->bstrVal, NULL, 10));
        }

        ntl::ComVariant QueryInstanceName(_In_ IWbemObjectAccess* _instance)
        {
            return ReadIWbemObjectAccess(_instance, L"Name");
        }
        ntl::ComVariant QueryInstanceName(_In_ IWbemClassObject* _instance)
        {
            ntl::ComVariant value;
            HRESULT hr = _instance->Get(L"Name", 0, value.get(), nullptr, nullptr);
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"IWbemClassObject::Get(Name)", L"WmiPerformanceCounterData::match", false);
            }
            return value;
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        ///
        /// type for the callback implemented in all WmiPerformanceCounter classes
        ///
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        enum class CallbackAction {
            Start,
            Stop,
            Update,
            Clear
        };
        typedef std::function<void(const CallbackAction _action)> WmiPerformanceCallback;
    } // unnamed namespace


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// class WmiPerformanceCounter
    /// - The abstract base class contains the WMI-specific code which all templated instances will derive from
    /// - Using public inheritance + protected members over composition as we need a common type which we can pass to 
    ///   WmiPerformance 
    /// - Exposes the iterator class for users to traverse the data points gathered
    ///
    /// Note: callers *MUST* guarantee connections with the WMI service stay connected
    ///       for the lifetime of this object [e.g. guarnated WmiService is instanitated]
    /// Note: callers *MUST* guarantee that COM is CoInitialized on this thread before calling
    /// Note: the WmiPerformance class *will* retain WMI service instance
    ///       it's recommended to guarantee it stays alive
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ///
    /// forward-declaration to reference WmiPerformance
    ///
    class WmiPerformance;
    template <typename T>
    class WmiPerformanceCounter {
    public:
        ///
        /// iterator
        ///
        /// iterates across *time-slices* captured over from WmiPerformance
        ///
        class iterator : public std::iterator<std::forward_iterator_tag, T> {
        private:
            typename std::vector<T>::const_iterator current;
            bool is_empty;

        public:
            explicit iterator(typename std::vector<T>::const_iterator && _instance)
            : current(std::move(_instance)),
              is_empty(false)
            {
            }
            iterator() : current(), is_empty(true)
            {
            }
            ~iterator() = default;

            ////////////////////////////////////////////////////////////////////////////////
            ///
            /// copy c'tor and copy assignment
            /// move c'tor and move assignment
            ///
            ////////////////////////////////////////////////////////////////////////////////
            iterator(_In_ iterator&& _i) NOEXCEPT
            : current(std::move(_i.current)),
              is_empty(std::move(_i.is_empty))
            {
            }
            iterator& operator =(_In_ iterator&& _i) NOEXCEPT
            {
                current = std::move(_i.current);
                is_empty = std::move(_i.is_empty);
                return *this;
            }

            iterator(const iterator& _i) NOEXCEPT
            : current(_i.current),
              is_empty(_i.is_empty)
            {
            }
            iterator& operator =(const iterator& i) NOEXCEPT
            {
                iterator local_copy(i);
                *this = std::move(local_copy);
                return *this;
            }

            const T& operator* () const
            {
                if (is_empty) {
                    throw std::runtime_error("WmiPerformanceCounter::iterator : dereferencing an iterator referencing an empty container");
                }
                return *current;
            }

            /*
            T* operator-> ()
            {
                if (is_empty) {
                    throw std::runtime_error("WmiPerformanceCounter::iterator : dereferencing an iterator referencing an empty container");
                }
                &(current);
            }
            */

            bool operator==(const iterator& _iter) const NOEXCEPT
            {
                if (is_empty || _iter.is_empty) {
                    return (is_empty == _iter.is_empty);
                } else {
                    return (current == _iter.current);
                }
            }
            bool operator!=(const iterator& _iter) const NOEXCEPT
            {
                return !(*this == _iter);
            }

            // preincrement
            iterator& operator++()
            {
                if (is_empty) {
                    throw std::runtime_error("WmiPerformanceCounter::iterator : preincrementing an iterator referencing an empty container");
                }
                ++current;
                return *this;
            }
            // postincrement
            iterator operator++(_In_ int)
            {
                if (is_empty) {
                    throw std::runtime_error("WmiPerformanceCounter::iterator : postincrementing an iterator referencing an empty container");
                }
                iterator temp(*this);
                ++current;
                return temp;
            }
            // increment by integer
            iterator& operator+=(_In_ size_t _inc)
            {
                if (is_empty) {
                    throw std::runtime_error("WmiPerformanceCounter::iterator : postincrementing an iterator referencing an empty container");
                }
                for (size_t loop = 0; loop < _inc; ++loop) {
                    ++current;
                }
                return *this;
            }
        };

    public:
        WmiPerformanceCounter(_In_ LPCWSTR _counter_name, const WmiPerformanceCollectionType _collection_type)
        : collection_type(_collection_type),
          counter_name(_counter_name)
        {
            refresher = ntl::ComPtr<IWbemRefresher>::createInstance(CLSID_WbemRefresher, IID_IWbemRefresher);
            HRESULT hr = refresher->QueryInterface(IID_IWbemConfigureRefresher, reinterpret_cast<void**>(configure_refresher.get_addr_of()));
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"IWbemRefresher::QueryInterface", L"WmiPerformanceCounter", false);
            }
        }

        virtual ~WmiPerformanceCounter() = default;

        WmiPerformanceCounter(const WmiPerformanceCounter&) = delete;
        WmiPerformanceCounter& operator=(const WmiPerformanceCounter&) = delete;
        WmiPerformanceCounter(WmiPerformanceCounter&&) = delete;
        WmiPerformanceCounter& operator=(WmiPerformanceCounter&&) = delete;

        ///
        /// *not* thread-safe: caller must guarantee sequential access to add_filter()
        ///
        template <typename T>
        void add_filter(_In_ LPCWSTR _counter_name, _In_ T _property_value)
        {
            ntl::FatalCondition(
                !data_stopped,
                L"WmiPerformanceCounter: must call stop_all_counters on the WmiPerformance class containing this counter");
            instance_filter.emplace_back(_counter_name, WmiMakeVariant(_property_value));
        }

        ///
        /// returns a begin/end pair of interator that exposes data for each time-slice
        /// - static classes will have a null instance name
        ///
        std::pair<iterator, iterator> reference_range(_In_ LPCWSTR _instance_name = nullptr)
        {
            ntl::FatalCondition(
                !data_stopped,
                L"WmiPerformanceCounter: must call stop_all_counters on the WmiPerformance class containing this counter");

            auto found_instance = std::find_if(
                std::begin(counter_data),
                std::end(counter_data),
                [&] (const std::unique_ptr<WmiPeformanceCounterData<T>>& _instance) {
                return _instance->match(_instance_name);
            });
            if (std::end(counter_data) == found_instance) {
                // nothing matching that instance name
                // return the end iterator (default c'tor == end)
                return std::pair<iterator, iterator>(iterator(), iterator());
            }

            const std::unique_ptr<WmiPeformanceCounterData<T>>& instance_reference = *found_instance;
            return std::pair<iterator, iterator>(instance_reference->begin(), instance_reference->end());
        }

    private:
        //
        // private stucture to track the 'filter' which instances to track
        //
        struct WmiPerformanceInstanceFilter {
            const std::wstring counter_name;
            const ntl::ComVariant property_value;

            WmiPerformanceInstanceFilter(_In_ LPCWSTR _counter_name, const ntl::ComVariant& _property_value)
            : counter_name(_counter_name),
              property_value(_property_value)
            {
            }
            ~WmiPerformanceInstanceFilter() = default;

            bool operator==(_In_ IWbemObjectAccess* _instance)
            {
                return (property_value == ntl::ReadIWbemObjectAccess(_instance, counter_name.c_str()));
            }
            bool operator!=(_In_ IWbemObjectAccess* _instance)
            {
                return !(*this == _instance);
            }

            bool operator==(_In_ IWbemClassObject* _instance)
            {
                ntl::ComVariant value;
                HRESULT hr = _instance->Get(counter_name.c_str(), 0, value.get(), nullptr, nullptr);
                if (FAILED(hr)) {
                    throw ntl::Exception(hr, L"IWbemClassObject::Get(counter_name)", L"WmiPerformanceCounterData", false);
                }
                // if the filter currently doesn't match anything we have, return not equal
                if (value->vt == VT_NULL)
                {
                    return false;
                }
                FatalCondition(
                    value->vt != property_value->vt,
                    L"VARIANT types do not match to make a comparison : Counter name '%s', retrieved type '%u', expected type '%u'",
                    counter_name.c_str(), value->vt, property_value->vt);

                return (property_value == value);
            }
            bool operator!=(_In_ IWbemClassObject* _instance)
            {
                return !(*this == _instance);
            }
        };

        const WmiPerformanceCollectionType collection_type;
        const std::wstring counter_name;
        ntl::ComPtr<IWbemRefresher> refresher;
        ntl::ComPtr<IWbemConfigureRefresher> configure_refresher;
        std::vector<WmiPerformanceInstanceFilter> instance_filter;
        std::vector<std::unique_ptr<WmiPeformanceCounterData<T>>> counter_data;
        bool data_stopped = true;

    protected:

        virtual void update_counter_data() = 0;

        // WmiPerformance needs private access to invoke register_callback in the derived type
        friend class WmiPerformance;
        WmiPerformanceCallback register_callback()
        {
            return [this] (const CallbackAction _update_data) {
                switch (_update_data)
                {
                case CallbackAction::Start:
                    data_stopped = false;
                    break;

                case CallbackAction::Stop:
                    data_stopped = true;
                    break;

                case CallbackAction::Update:
                    // only the derived class has appropriate the accessor class to update the data
                    update_counter_data();
                    break;

                case CallbackAction::Clear:
                    ntl::FatalCondition(
                        !data_stopped,
                        L"WmiPerformanceCounter: must call stop_all_counters on the WmiPerformance class containing this counter");

                    for (auto& _counter_data : counter_data) {
                        _counter_data->clear();
                    }
                    break;
                }
            };
        }

        //
        // the derived classes need to use the same refresher object
        //
        ntl::ComPtr<IWbemConfigureRefresher> access_refresher()
        {
            return configure_refresher;
        }

        //
        // this function is how one looks to see if the data machines requires knowing how to access the data from that specific WMI class
        // - it's also how to access the data is captured with the TAccess template type
        //
        template <typename TAccess>
        void add_instance(_In_ TAccess* _instance)
        {
            bool fAddData = instance_filter.empty();
            if (!fAddData) {
                fAddData = (std::end(instance_filter) != std::find(
                    std::begin(instance_filter),
                    std::end(instance_filter),
                    _instance));
            }

            // add the counter data for this instance if:
            // - have no filters [not filtering instances at all]
            // - matches at least one filter
            if (fAddData) {
                ntl::ComVariant instance_name = QueryInstanceName(_instance);

                auto tracked_instance = std::find_if(
                    std::begin(counter_data),
                    std::end(counter_data),
                    [&] (std::unique_ptr<WmiPeformanceCounterData<T>>& _counter_data) {
                    return _counter_data->match(instance_name->bstrVal);
                });

                // if this instance of this counter is new [new unique instance for this counter]
                // - we must add a new WmiPeformanceCounterData to the parent's counter_data vector
                // else
                // - we just add this counter value to the already-tracked instance
                if (tracked_instance == std::end(counter_data)) {
                    counter_data.push_back(
                        std::unique_ptr<WmiPeformanceCounterData<T>>
                            (new WmiPeformanceCounterData<T>(collection_type, _instance, counter_name.c_str())));
                    (*counter_data.rbegin())->add(_instance);
                } else {
                    (*tracked_instance)->add(_instance);
                }
            }
        }
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// WmiPerformanceCounterImpl
    /// - derived from the pure-virtual WmiPerformanceCounter class
    ///   shares the same data type template typename with the parent class
    ///
    /// Template typename details:
    ///
    /// - TEnum: the IWbem* type to refresh the performance data
    /// - TAccess: the IWbem* type used to access the performance data once refreshed
    /// - TData: the data type of the counter of the class specified in the c'tor
    ///
    /// Only 2 combinations currently supported:
    /// : WmiPerformanceCounter<IWbemHiPerfEnum, IWbemObjectAccess, TData>
    ///   - refreshes N of instances of a counter
    /// : WmiPerformanceCounter<IWbemClassObject, IWbemClassObject, TData>
    ///   - refreshes a single instance of a counter
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename TEnum, typename TAccess, typename TData>
    class WmiPerformanceCounterImpl : public WmiPerformanceCounter<TData> {
    public:
        WmiPerformanceCounterImpl(_In_ LPCWSTR _class_name, _In_ LPCWSTR _counter_name, const WmiPerformanceCollectionType _collection_type)
        : WmiPerformanceCounter<TData>(_counter_name, _collection_type),
          accessor(access_refresher(), _class_name)
        {
        }

        ~WmiPerformanceCounterImpl() = default;

        // non-copyable
        WmiPerformanceCounterImpl(const WmiPerformanceCounterImpl&) = delete;
        WmiPerformanceCounterImpl& operator=(const WmiPerformanceCounterImpl&) = delete;

    private:
        ///
        /// this concrete template class serves to capture the Enum and Access template types
        /// - so can instantiate the appropriate accessor object
        WmiPerformanceDataAccessor<TEnum, TAccess> accessor;

        ///
        /// invoked from the parent class to add data matching any/all filters
        ///
        /// private function required to be implemented from the abstract base class
        /// - concrete classe must pass back a function callback for adding data points for the specified counter
        ///
        void update_counter_data()
        {
            // refresh this hi-perf object to get the current values
            // requires the invoker serializes all calls
            accessor.refresh();

            // the accessor object exposes begin() / end() to allow access to instances
            // - of the specified hi-performance counter
            for (auto& _instance : accessor) {
                add_instance(_instance);
            }
        }
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// WmiPerformance
    ///
    /// class to register for and collect performance counters
    /// - captures counter data into the WmiPerformanceCounter objects passed through add()
    ///
    /// CAUTION:
    /// - do not access the WmiPerformanceCounter instances while between calling start() and stop()
    /// - any iterators returned can be invalidated when more data is added on the next cycle
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    class WmiPerformance {
    public:
        WmiPerformance() : wmi_service(L"root\\cimv2")
        {
            refresher = ntl::ComPtr<IWbemRefresher>::createInstance(CLSID_WbemRefresher, IID_IWbemRefresher);
            HRESULT hr = refresher->QueryInterface(
                IID_IWbemConfigureRefresher,
                reinterpret_cast<void**>(config_refresher.get_addr_of()));
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"IWbemRefresher::QueryInterface(IID_IWbemConfigureRefresher)", L"WmiPerformance", false);
            }
        }

        virtual ~WmiPerformance() NOEXCEPT
        {
            stop_all_counters();
        }

        template <typename T>
        void add_counter(const std::shared_ptr<WmiPerformanceCounter<T>>& _wmi_perf)
        {
            callbacks.push_back(_wmi_perf->register_callback());
            ScopeGuard(revert_callback, { callbacks.pop_back(); });

            HRESULT hr = config_refresher->AddRefresher(_wmi_perf->refresher.get(), 0, nullptr);
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"IWbemConfigureRefresher::AddRefresher", L"WmiPerformance<T>::add", false);
            }

            // dismiss scope-guard - successfully added refresher
            revert_callback.dismiss();
        }

        void start_all_counters(unsigned _interval)
        {
            for (auto& _callback : callbacks) {
                _callback(CallbackAction::Start);
            }
            timer.reset(new ntl::ThreadpoolTimer);
            timer->schedule_singleton(
                [this, _interval] () { TimerCallback(this, _interval); },
                _interval);
        }
        // no-throw / no-fail
        void stop_all_counters() NOEXCEPT
        {
            if (timer) {
                timer->stop_all_timers();
            }
            for (auto& _callback : callbacks) {
                _callback(CallbackAction::Stop);
            }
        }

        // no-throw / no-fail
        void clear_counter_data() NOEXCEPT
        {
            for (auto& _callback : callbacks) {
                _callback(CallbackAction::Clear);
            }
        }

        void reset_counters()
        {
            callbacks.clear();

            // release this Refresher and ConfigRefresher, so future counters will be added cleanly
            refresher = ntl::ComPtr<IWbemRefresher>::createInstance(CLSID_WbemRefresher, IID_IWbemRefresher);
            HRESULT hr = refresher->QueryInterface(
                IID_IWbemConfigureRefresher,
                reinterpret_cast<void**>(config_refresher.get_addr_of()));
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"IWbemRefresher::QueryInterface(IID_IWbemConfigureRefresher)", L"WmiPerformance", false);
            }
        }

        // non-copyable
        WmiPerformance(const WmiPerformance&) = delete;
        WmiPerformance& operator=(const WmiPerformance&) = delete;

        // movable
        WmiPerformance(WmiPerformance&& rhs) :
            com_init(),
            wmi_service(std::move(rhs.wmi_service)),
            refresher(std::move(rhs.refresher)),
            config_refresher(std::move(rhs.config_refresher)),
            callbacks(std::move(rhs.callbacks)),
            timer(std::move(rhs.timer))
        {
        }

    private:
        ntl::ComInitialize com_init;
        WmiService wmi_service;
        ntl::ComPtr<IWbemRefresher> refresher;
        ntl::ComPtr<IWbemConfigureRefresher> config_refresher;
        // for each interval, callback each of the registered aggregators
        std::vector<WmiPerformanceCallback> callbacks;
        // timer to fire to indicate when to Refresh the data
        // declare last to guarantee will be destroyed first
        std::unique_ptr<ntl::ThreadpoolTimer> timer;

        static void TimerCallback(ntl::WmiPerformance* this_ptr, unsigned long _interval)
        {
            try {
                // must guarantee COM is initialized on this thread
                ntl::ComInitialize com;
                this_ptr->refresher->Refresh(0);

                for (const auto& _callback : this_ptr->callbacks) {
                    _callback(CallbackAction::Update);
                }

                this_ptr->timer->schedule_singleton(
                    [this_ptr, _interval] () { TimerCallback(this_ptr, _interval); }, 
                    _interval);
            }
            catch (const ntl::Exception& e) {
                ntl::AlwaysFatalCondition(L"Failed to schedule the next Performance Counter read [%ws : %u]", e.what_w(), e.why());
            }
            catch (const std::exception& e) {
                ntl::AlwaysFatalCondition(L"Failed to schedule the next Performance Counter read [%hs]", e.what());
            }
        }
    };


    enum class WmiClassType {
        Static, // MakeStaticPerfCounter
        Instance // created with MakeStaticPerfCounter
    };
    
    enum class WmiClassName {
        Process,
        Processor,
        Memory,
        NetworkAdapter,
        NetworkInterface,
        Tcpip_Diagnostics,
        Tcpip_Ipv4,
        Tcpip_Ipv6,
        Tcpip_TCPv4,
        Tcpip_TCPv6,
        Tcpip_UDPv4,
        Tcpip_UDPv6,
        WinsockBSP
    };

    struct WmiPerformanceCounterProperties {
        const WmiClassType classType;
        const WmiClassName className;
        const wchar_t* providerName;

        const unsigned long ulongFieldNameCount;
        const wchar_t** ulongFieldNames;

        const unsigned long ulonglongFieldNameCount;
        const wchar_t** ulonglongFieldNames;

        const unsigned long stringFieldNameCount;
        const wchar_t** stringFieldNames;

        template <typename T> bool PropertyNameExists(_In_ LPCWSTR name) const NOEXCEPT;
    };

    template <> 
    bool WmiPerformanceCounterProperties::PropertyNameExists<ULONG>(_In_ LPCWSTR name) const NOEXCEPT
    {
        for (unsigned counter = 0; counter < this->ulongFieldNameCount; ++counter) {
            if (ntl::String::iordinal_equals(name, this->ulongFieldNames[counter])) {
                return true;
            }
        }

        return false;
    }
    template <>
    bool WmiPerformanceCounterProperties::PropertyNameExists<ULONGLONG>(_In_ LPCWSTR name) const NOEXCEPT
    {
        for (unsigned counter = 0; counter < this->ulonglongFieldNameCount; ++counter) {
            if (ntl::String::iordinal_equals(name, this->ulonglongFieldNames[counter])) {
                return true;
            }
        }

        return false;
    }
    template <>
    bool WmiPerformanceCounterProperties::PropertyNameExists<std::wstring>(_In_ LPCWSTR name) const NOEXCEPT
    {
        for (unsigned counter = 0; counter < this->stringFieldNameCount; ++counter) {
            if (ntl::String::iordinal_equals(name, this->stringFieldNames[counter])) {
                return true;
            }
        }

        return false;
    }
    template <>
    bool WmiPerformanceCounterProperties::PropertyNameExists<ntl::ComBstr>(_In_ LPCWSTR name) const NOEXCEPT
    {
        for (unsigned counter = 0; counter < this->stringFieldNameCount; ++counter) {
            if (ntl::String::iordinal_equals(name, this->stringFieldNames[counter])) {
                return true;
            }
        }

        return false;
    }

    namespace WmiPerformanceDetails {
        const wchar_t* CommonStringPropertyNames[] = {
            L"Caption",
            L"Description",
            L"Name" };

        const wchar_t* Memory_Counter = L"Win32_PerfFormattedData_PerfOS_Memory";
        const wchar_t* Memory_UlongCounterNames[] = {
            L"CacheFaultsPerSec",
            L"DemandZeroFaultsPerSec",
            L"FreeSystemPageTableEntries",
            L"PageFaultsPerSec",
            L"PageReadsPerSec",
            L"PagesInputPerSec",
            L"PagesOutputPerSec",
            L"PagesPerSec",
            L"PageWritesPerSec",
            L"PerceCommittedBytesInUse",
            L"PoolNonpagedAllocs",
            L"PoolPagedAllocs",
            L"TransitionFaultsPerSec",
            L"WriteCopiesPerSec"
        };
        const wchar_t* Memory_UlonglongCounterNames[] = {
            L"AvailableBytes",
            L"AvailableKBytes",
            L"AvailableMBytes",
            L"CacheBytes",
            L"CacheBytesPeak",
            L"CommitLimit",
            L"CommittedBytes",
            L"Frequency_Object",
            L"Frequency_PerfTime",
            L"Frequency_Sys100NS",
            L"PoolNonpagedBytes",
            L"PoolPagedBytes",
            L"PoolPagedResidentBytes",
            L"SystemCacheResidentBytes",
            L"SystemCodeResidentBytes",
            L"SystemCodeTotalBytes",
            L"SystemDriverResidentBytes",
            L"SystemDriverTotalBytes",
            L"Timestamp_Object",
            L"Timestamp_PerfTime",
            L"Timestamp_Sys100NS"
        };

        const wchar_t* ProcessorInformation_Counter = L"Win32_PerfFormattedData_Counters_ProcessorInformation";
        const wchar_t* ProcessorInformation_UlongCounterNames[] = {
            L"ClockInterruptsPersec",
            L"DPCRate",
            L"DPCsQueuedPersec",
            L"InterruptsPersec",
            L"ParkingStatus",
            L"PercentofMaximumFrequency",
            L"PercentPerformanceLimit",
            L"PerformanceLimitFlags",
            L"ProcessorFrequency",
            L"ProcessorStateFlags"
        };
        const wchar_t* ProcessorInformation_UlonglongCounterNames[] = {
            L"AverageIdleTime",
            L"C1TransitionsPerSec",
            L"C2TransitionsPerSec",
            L"C3TransitionsPerSec",
            L"IdleBreakEventsPersec",
            L"PercentC1Time",
            L"PercentC2Time",
            L"PercentC3Time",
            L"PercentDPCTime",
            L"PercentIdleTime",
            L"PercentInterruptTime",
            L"PercentPriorityTime",
            L"PercentPrivilegedTime",
            L"PercentPrivilegedUtility",
            L"PercentProcessorPerformance",
            L"PercentProcessorTime",
            L"PercentProcessorUtility",
            L"PercentUserTime",
            L"Timestamp_Object",
            L"Timestamp_PerfTime",
            L"Timestamp_Sys100NS"
        };

        const wchar_t* PerfProc_Process_Counter = L"Win32_PerfFormattedData_PerfProc_Process";
        const wchar_t* PerfProc_Process_UlongCounterNames[] = {
            L"CreatingProcessID",
            L"HandleCount",
            L"IDProcess",
            L"PageFaultsPerSec",
            L"PoolNonpagedBytes",
            L"PoolPagedBytes",
            L"PriorityBase",
            L"ThreadCount"
        };
        const wchar_t* PerfProc_Process_UlonglongCounterNames[] = {
            L"ElapsedTime",
            L"Frequency_Object",
            L"Frequency_PerfTime",
            L"Frequency_Sys100NS",
            L"IODataBytesPerSec",
            L"IODataOperationsPerSec",
            L"IOOtherBytesPerSec",
            L"IOOtherOperationsPerSec",
            L"IOReadBytesPerSec",
            L"IOReadOperationsPerSec",
            L"IOWriteBytesPerSec",
            L"IOWriteOperationsPerSec",
            L"PageFileBytes",
            L"PageFileBytesPeak",
            L"PercentPrivilegedTime",
            L"PercentProcessorTime",
            L"PercentUserTime",
            L"PrivateBytes",
            L"Timestamp_Object",
            L"Timestamp_PerfTime",
            L"Timestamp_Sys100NS",
            L"VirtualBytes",
            L"VirtualBytesPeak",
            L"WorkingSet",
            L"WorkingSetPeak"
        };

        const wchar_t* Tcpip_NetworkAdapter_Counter = L"Win32_PerfFormattedData_Tcpip_NetworkAdapter";
        const wchar_t* Tcpip_NetworkAdapter_ULongLongCounterNames[] = {
            L"BytesReceivedPersec",
            L"BytesSentPersec",
            L"BytesTotalPersec",
            L"CurrentBandwidth",
            L"OffloadedConnections",
            L"OutputQueueLength",
            L"PacketsOutboundDiscarded",
            L"PacketsOutboundErrors",
            L"PacketsReceivedDiscarded",
            L"PacketsReceivedErrors",
            L"PacketsReceivedNonUnicastPersec",
            L"PacketsReceivedUnicastPersec",
            L"PacketsReceivedUnknown",
            L"PacketsReceivedPersec",
            L"PacketsSentNonUnicastPersec",
            L"PacketsSentUnicastPersec",
            L"PacketsSentPersec",
            L"PacketsPersec",
            L"TCPActiveRSCConnections",
            L"TCPRSCAveragePacketSize",
            L"TCPRSCCoalescedPacketsPersec",
            L"TCPRSCntl::ExceptionsPersec",
            L"Timestamp_Object",
            L"Timestamp_PerfTime",
            L"Timestamp_Sys100NS"
        };

        const wchar_t* Tcpip_NetworkInterface_Counter = L"Win32_PerfFormattedData_Tcpip_NetworkInterface";
        const wchar_t* Tcpip_NetworkInterface_ULongLongCounterNames[] = {
            L"BytesReceivedPerSec",
            L"BytesSentPerSec",
            L"BytesTotalPerSec",
            L"CurrentBandwidth",
            L"Frequency_Object",
            L"Frequency_PerfTime",
            L"Frequency_Sys100NS",
            L"OffloadedConnections",
            L"OutputQueueLength",
            L"PacketsOutboundDiscarded",
            L"PacketsOutboundErrors",
            L"PacketsPerSec",
            L"PacketsReceivedDiscarded",
            L"PacketsReceivedErrors",
            L"PacketsReceivedNonUnicastPerSec",
            L"PacketsReceivedPerSec",
            L"PacketsReceivedUnicastPerSec",
            L"PacketsReceivedUnknown",
            L"PacketsSentNonUnicastPerSec",
            L"PacketsSentPerSec",
            L"PacketsSentUnicastPerSec"
            L"TCPActiveRSCConnections",
            L"TCPRSCAveragePacketSize",
            L"TCPRSCCoalescedPacketsPersec",
            L"TCPRSCExceptionsPersec"
            L"Timestamp_Object",
            L"Timestamp_PerfTime",
            L"Timestamp_Sys100NS"
        };

        const wchar_t* Tcpip_IPv4_Counter = L"Win32_PerfFormattedData_Tcpip_IPv4";
        const wchar_t* Tcpip_IPv6_Counter = L"Win32_PerfFormattedData_Tcpip_IPv6";
        const wchar_t* Tcpip_IP_ULongCounterNames[] = {
            L"DatagramsForwardedPersec",
            L"DatagramsOutboundDiscarded",
            L"DatagramsOutboundNoRoute",
            L"DatagramsReceivedAddressErrors",
            L"DatagramsReceivedDeliveredPersec",
            L"DatagramsReceivedDiscarded",
            L"DatagramsReceivedHeaderErrors",
            L"DatagramsReceivedUnknownProtocol",
            L"DatagramsReceivedPersec",
            L"DatagramsSentPersec",
            L"DatagramsPersec",
            L"FragmentReassemblyFailures",
            L"FragmentationFailures",
            L"FragmentedDatagramsPersec",
            L"FragmentsCreatedPersec",
            L"FragmentsReassembledPersec",
            L"FragmentsReceivedPersec"
        };

        const wchar_t* Tcpip_TCPv4_Counter = L"Win32_PerfFormattedData_Tcpip_TCPv4";
        const wchar_t* Tcpip_TCPv6_Counter = L"Win32_PerfFormattedData_Tcpip_TCPv6";
        const wchar_t* Tcpip_TCP_ULongCounterNames[] = {
            L"ConnectionFailures",
            L"ConnectionsActive",
            L"ConnectionsEstablished",
            L"ConnectionsPassive",
            L"ConnectionsReset",
            L"SegmentsReceivedPersec",
            L"SegmentsRetransmittedPersec",
            L"SegmentsSentPersec",
            L"SegmentsPersec"
        };

        const wchar_t* Tcpip_UDPv4_Counter = L"Win32_PerfFormattedData_Tcpip_UDPv4";
        const wchar_t* Tcpip_UDPv6_Counter = L"Win32_PerfFormattedData_Tcpip_UDPv6";
        const wchar_t* Tcpip_UDP_ULongCounterNames[] = {
            L"DatagramsNoPortPersec",
            L"DatagramsReceivedErrors",
            L"DatagramsReceivedPersec",
            L"DatagramsSentPersec",
            L"DatagramsPersec"
        };

        const wchar_t* TCPIPPerformanceDiagnostics_Counter = L"Win32_PerfFormattedData_TCPIPCounters_TCPIPPerformanceDiagnostics";
        const wchar_t* TCPIPPerformanceDiagnostics_ULongCounterNames[] = {
            L"Deniedconnectorsendrequestsinlowpowermode",
            L"IPv4NBLsindicatedwithlowresourceflag",
            L"IPv4NBLsindicatedwithoutprevalidation",
            L"IPv4NBLstreatedasnonprevalidated",
            L"IPv4NBLsPersecindicatedwithlowresourceflag",
            L"IPv4NBLsPersecindicatedwithoutprevalidation",
            L"IPv4NBLsPersectreatedasnonprevalidated",
            L"IPv4outboundNBLsnotprocessedviafastpath",
            L"IPv4outboundNBLsPersecnotprocessedviafastpath",
            L"IPv6NBLsindicatedwithlowresourceflag",
            L"IPv6NBLsindicatedwithoutprevalidation",
            L"IPv6NBLstreatedasnonprevalidated",
            L"IPv6NBLsPersecindicatedwithlowresourceflag",
            L"IPv6NBLsPersecindicatedwithoutprevalidation",
            L"IPv6NBLsPersectreatedasnonprevalidated",
            L"IPv6outboundNBLsnotprocessedviafastpath",
            L"IPv6outboundNBLsPersecnotprocessedviafastpath",
            L"TCPconnectrequestsfallenoffloopbackfastpath",
            L"TCPconnectrequestsPersecfallenoffloopbackfastpath",
            L"TCPinboundsegmentsnotprocessedviafastpath",
            L"TCPinboundsegmentsPersecnotprocessedviafastpath"
        };

        const wchar_t* MicrosoftWinsockBSP_Counter = L"Win32_PerfFormattedData_AFDCounters_MicrosoftWinsockBSP";
        const wchar_t* MicrosoftWinsockBSP_ULongCounterNames[] = {
            L"DroppedDatagrams",
            L"DroppedDatagramsPersec",
            L"RejectedConnections",
            L"RejectedConnectionsPersec"
        };

        // this patterns (const array of wchar_t* pointers) 
        // allows for compile-time construction of the array of properties
        const WmiPerformanceCounterProperties PerformanceCounterPropertiesArray[] = {

        {
            WmiClassType::Static,
            WmiClassName::Memory,
            Memory_Counter,
            ARRAYSIZE(Memory_UlongCounterNames),
            Memory_UlongCounterNames,
            ARRAYSIZE(Memory_UlonglongCounterNames),
            Memory_UlonglongCounterNames,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Instance,
            WmiClassName::Processor,
            ProcessorInformation_Counter,
            ARRAYSIZE(ProcessorInformation_UlongCounterNames),
            ProcessorInformation_UlongCounterNames,
            ARRAYSIZE(ProcessorInformation_UlonglongCounterNames),
            ProcessorInformation_UlonglongCounterNames,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Instance,
            WmiClassName::Process,
            PerfProc_Process_Counter,
            ARRAYSIZE(PerfProc_Process_UlongCounterNames),
            PerfProc_Process_UlongCounterNames,
            ARRAYSIZE(PerfProc_Process_UlonglongCounterNames),
            PerfProc_Process_UlonglongCounterNames,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Instance,
            WmiClassName::NetworkAdapter,
            Tcpip_NetworkAdapter_Counter,
            0,
            nullptr,
            ARRAYSIZE(Tcpip_NetworkAdapter_ULongLongCounterNames),
            Tcpip_NetworkAdapter_ULongLongCounterNames,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Instance,
            WmiClassName::NetworkInterface,
            Tcpip_NetworkInterface_Counter,
            0,
            nullptr,
            ARRAYSIZE(Tcpip_NetworkInterface_ULongLongCounterNames),
            Tcpip_NetworkInterface_ULongLongCounterNames,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Static,
            WmiClassName::Tcpip_Ipv4,
            Tcpip_IPv4_Counter,
            ARRAYSIZE(Tcpip_IP_ULongCounterNames),
            Tcpip_IP_ULongCounterNames,
            0,
            nullptr,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Static,
            WmiClassName::Tcpip_Ipv6,
            Tcpip_IPv6_Counter,
            ARRAYSIZE(Tcpip_IP_ULongCounterNames),
            Tcpip_IP_ULongCounterNames,
            0,
            nullptr,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Static,
            WmiClassName::Tcpip_TCPv4,
            Tcpip_TCPv4_Counter,
            ARRAYSIZE(Tcpip_TCP_ULongCounterNames),
            Tcpip_TCP_ULongCounterNames,
            0,
            nullptr,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Static,
            WmiClassName::Tcpip_TCPv6,
            Tcpip_TCPv6_Counter,
            ARRAYSIZE(Tcpip_TCP_ULongCounterNames),
            Tcpip_TCP_ULongCounterNames,
            0,
            nullptr,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Static,
            WmiClassName::Tcpip_UDPv4,
            Tcpip_UDPv4_Counter,
            ARRAYSIZE(Tcpip_UDP_ULongCounterNames),
            Tcpip_UDP_ULongCounterNames,
            0,
            nullptr,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Static,
            WmiClassName::Tcpip_UDPv6,
            Tcpip_UDPv6_Counter,
            ARRAYSIZE(Tcpip_UDP_ULongCounterNames),
            Tcpip_UDP_ULongCounterNames,
            0,
            nullptr,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Static,
            WmiClassName::Tcpip_Diagnostics,
            TCPIPPerformanceDiagnostics_Counter,
            ARRAYSIZE(TCPIPPerformanceDiagnostics_ULongCounterNames),
            TCPIPPerformanceDiagnostics_ULongCounterNames,
            0,
            nullptr,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        },

        {
            WmiClassType::Static,
            WmiClassName::WinsockBSP,
            MicrosoftWinsockBSP_Counter,
            ARRAYSIZE(MicrosoftWinsockBSP_ULongCounterNames),
            MicrosoftWinsockBSP_ULongCounterNames,
            0,
            nullptr,
            ARRAYSIZE(CommonStringPropertyNames),
            CommonStringPropertyNames
        }

        };
    }

    template <typename T>
    std::shared_ptr<WmiPerformanceCounter<T>> MakeStaticPerfCounter(_In_ LPCWSTR _class_name, _In_ LPCWSTR _counter_name, const WmiPerformanceCollectionType _collection_type = WmiPerformanceCollectionType::Detailed)
    {
        // 'static' WMI PerfCounters enumerate via IWbemClassObject and accessed/refreshed via IWbemClassObject
        return std::make_shared<WmiPerformanceCounterImpl<IWbemClassObject, IWbemClassObject, T>>(_class_name, _counter_name, _collection_type);
    }

    template <typename T>
    std::shared_ptr<WmiPerformanceCounter<T>> MakeInstancePerfCounter(_In_ LPCWSTR _class_name, _In_ LPCWSTR _counter_name, const WmiPerformanceCollectionType _collection_type = WmiPerformanceCollectionType::Detailed)
    {
        // 'instance' WMI perf objects are enumerated through the IWbemHiPerfEnum interface and accessed/refreshed through the IWbemObjectAccess interface
        return std::make_shared<WmiPerformanceCounterImpl<IWbemHiPerfEnum, IWbemObjectAccess, T>>(_class_name, _counter_name, _collection_type);
    }

    template <typename T>
    std::shared_ptr<WmiPerformanceCounter<T>> CreatePerfCounter(const WmiClassName& _class, _In_ LPCWSTR _counter_name, const WmiPerformanceCollectionType _collection_type = WmiPerformanceCollectionType::Detailed)
    {
        const WmiPerformanceCounterProperties* foundProperty = nullptr;
        for (const auto& counterProperty : WmiPerformanceDetails::PerformanceCounterPropertiesArray) {
            if (_class == counterProperty.className) {
                foundProperty = &counterProperty;
                break;
            }
        }

        if (!foundProperty) {
            throw std::invalid_argument("Unknown WMI Performance Counter Class");
        }

        if (!foundProperty->PropertyNameExists<T>(_counter_name)) {
            throw ntl::Exception(
                ERROR_INVALID_DATA,
                String::format_string(
                    L"CounterName (%ws) does not exist in the requested class (%u)",
                    _counter_name, _class).c_str(),
                L"CreatePerfCounter",
                true);
        }

        if (foundProperty->classType == WmiClassType::Static) {
            return MakeStaticPerfCounter<T>(foundProperty->providerName, _counter_name, _collection_type);
        } else {
            return MakeInstancePerfCounter<T>(foundProperty->providerName, _counter_name, _collection_type);
        }
    }
} // ntl namespace
