// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <iterator>
#include <exception>
#include <stdexcept>
// os headers
#include <windows.h>
#include <Wbemidl.h>
// local headers
#include "ntlComInitialize.hpp"
#include "ntlWmiException.hpp"
#include "ntlWmiService.hpp"
#include "ntlVersionConversion.hpp"

namespace ntl
{

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// class WmiClassObject
///
/// Exposes enumerating properties of a WMI Provider through an property_iterator interface.
///
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class WmiClassObject
{
private:
    WmiService wbemServices;
    ntl::ComPtr<IWbemClassObject> wbemClass;

public:
    //
    // forward declare iterator classes
    //
    class property_iterator;
    class method_iterator;


    WmiClassObject(const WmiService& _wbemServices, const ntl::ComPtr<IWbemClassObject>& _wbemClass) :
        wbemServices(_wbemServices), 
        wbemClass(_wbemClass)
    {
    }

    WmiClassObject(const WmiService& _wbemServices, _In_ LPCWSTR _className) : 
        wbemServices(_wbemServices), 
        wbemClass()
    {
        HRESULT hr = this->wbemServices->GetObject(
            ntl::ComBstr(_className).get(),
            0,
            nullptr,
            this->wbemClass.get_addr_of(),
            nullptr);
        if (FAILED(hr)) {
            throw ntl::WmiException(hr, L"IWbemServices::GetObject", L"WmiClassObject::WmiClassObject", false);
        }
    }

    WmiClassObject(const WmiService& _wbemServices, const ntl::ComBstr& _className) : 
        wbemServices(_wbemServices), 
        wbemClass()
    {
        HRESULT hr = this->wbemServices->GetObjectW(
            _className.get(),
            0,
            nullptr,
            this->wbemClass.get_addr_of(),
            nullptr);
        if (FAILED(hr)) {
            throw ntl::WmiException(hr, L"IWbemServices::GetObject", L"WmiClassObject::WmiClassObject", false);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// Accessor to retrieve the encapsulated IWbemClassObject
    ///
    /// - returns the IWbemClassObject holding the class instance
    ///
    ////////////////////////////////////////////////////////////////////////////////
    ntl::ComPtr<IWbemClassObject> get_class_object() const NOEXCEPT
    {
        return this->wbemClass;
    }


    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// begin() and end() to return property property_iterators
    ///
    /// begin() arguments:
    ///   _wbemServices: a instance of IWbemServices
    ///   _className: a string of the class name which to enumerate
    ///   _fNonSystemPropertiesOnly: flag to control if should only enumerate
    ///       non-system properties
    ///
    /// begin() will throw an exception derived from std::exception on error
    /// end() is no-fail / no-throw
    ///
    ////////////////////////////////////////////////////////////////////////////////
    property_iterator property_begin(const bool _fNonSystemPropertiesOnly = true)
    {
        return property_iterator(this->wbemClass, _fNonSystemPropertiesOnly);
    }
    property_iterator property_end() NOEXCEPT
    {
        return property_iterator();
    }

    //
    // Not yet implemented
    //
    /// method_iterator method_begin(const bool _fLocalMethodsOnly = true)
    /// {
    ///     return method_iterator(this->wbemClass, _fLocalMethodsOnly);
    /// }
    /// method_iterator method_end() NOEXCEPT
    /// {
    ///     return method_iterator();
    /// }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// class WmiClassObject::property_iterator
    ///
    /// A forward property_iterator class type to enable forward-traversing instances of the queried
    ///  WMI provider
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    class property_iterator
    {
    private:
        static const unsigned long END_ITERATOR_INDEX = 0xffffffff;

        ntl::ComPtr<IWbemClassObject> wbemClassObj;
        ntl::ComBstr propName;
        CIMTYPE   propType;
        DWORD     dwIndex;

    public:
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// c'tor and d'tor
        /// - default c'tor is an 'end' property_iterator
        /// - traversal requires the callers IWbemServices interface and class name
        ///
        ////////////////////////////////////////////////////////////////////////////////
        property_iterator() NOEXCEPT : 
            wbemClassObj(), 
            propName(),
            propType(0),
            dwIndex(END_ITERATOR_INDEX)
        {
        }
        property_iterator(const ntl::ComPtr<IWbemClassObject>& _classObj, const bool _fNonSystemPropertiesOnly) :
            dwIndex(0), 
            wbemClassObj(_classObj), 
            propName(),
            propType(0)
        {
            HRESULT hr = wbemClassObj->BeginEnumeration((_fNonSystemPropertiesOnly) ? WBEM_FLAG_NONSYSTEM_ONLY : 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, wbemClassObj.get(), L"IWbemClassObject::BeginEnumeration", L"WmiClassObject::property_iterator::property_iterator", false);
            }
            
            increment();
        }

        ~property_iterator() NOEXCEPT
        {
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// copy c'tor and copy assignment
        ///
        /// Note that both are no-fail/no-throw operations
        ///
        ////////////////////////////////////////////////////////////////////////////////
        property_iterator(const property_iterator& _i) NOEXCEPT : 
            dwIndex(_i.dwIndex),
            wbemClassObj(_i.wbemClassObj),
            propName(_i.propName),
            propType(_i.propType)
        {
        }
        property_iterator& operator =(const property_iterator& _i) NOEXCEPT
        {
            property_iterator copy(_i);
            this->swap(copy);
            return *this;
        }

        void swap(_Inout_ property_iterator& _i) NOEXCEPT
        {
            using std::swap;
            swap(this->dwIndex, _i.dwIndex);
            swap(this->wbemClassObj, _i.wbemClassObj);
            swap(this->propName, _i.propName);
            swap(this->propType, _i.propType);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// accessors:
        /// - dereference operators to access the property name
        /// - explicit type() method to expose its CIM type
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ntl::ComBstr& operator*()
        {
            if (this->dwIndex == this->END_ITERATOR_INDEX) {
                throw std::out_of_range("WmiClassObject::property_iterator::operator - invalid subscript");
            }
            return this->propName;
        }
        const ntl::ComBstr& operator*() const
        {
            if (this->dwIndex == this->END_ITERATOR_INDEX) {
                throw std::out_of_range("WmiClassObject::property_iterator::operator - invalid subscript");
            }
            return this->propName;
        }
        ntl::ComBstr* operator->()
        {
            if (this->dwIndex == this->END_ITERATOR_INDEX) {
                throw std::out_of_range("WmiClassObject::property_iterator::operator-> - invalid subscript");
            }
            return &(this->propName);
        }
        const ntl::ComBstr* operator->() const
        {
            if (this->dwIndex == this->END_ITERATOR_INDEX) {
                throw std::out_of_range("WmiClassObject::property_iterator::operator-> - invalid subscript");
            }
            return &(this->propName);
        }
        CIMTYPE type() const
        {
            if (this->dwIndex == this->END_ITERATOR_INDEX) {
                throw std::out_of_range("WmiClassObject::property_iterator::type - invalid subscript");
            }
            return this->propType;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// comparison and arithmatic operators
        /// 
        /// comparison operators are no-throw/no-fail
        /// arithmatic operators can fail 
        /// - throwing a ntl::WmiException object capturing the WMI failures
        ///
        ////////////////////////////////////////////////////////////////////////////////
        bool operator==(const property_iterator& _iter) const NOEXCEPT
        {
            if (this->dwIndex != this->END_ITERATOR_INDEX) {
                return ( (this->dwIndex == _iter.dwIndex) && 
                         (this->wbemClassObj == _iter.wbemClassObj));
            } else {
                return ( this->dwIndex == _iter.dwIndex);
            }
        }
        bool operator!=(const property_iterator& _iter) const NOEXCEPT
        {
            return !(*this == _iter);
        }

        // preincrement
        property_iterator& operator++()
        {
            this->increment();
            return *this;
        }
        // postincrement
        property_iterator operator++(_In_ int)
        {
            property_iterator temp (*this);
            this->increment();
            return temp;
        }
        // increment by integer
        property_iterator& operator+=(_In_ DWORD _inc)
        {
            for (unsigned loop = 0; loop < _inc; ++loop) {
                this->increment();
                if (this->dwIndex == this->END_ITERATOR_INDEX) {
                    throw std::out_of_range("WmiClassObject::property_iterator::operator+= - invalid subscript");
                }
            }
            return *this;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// property_iterator_traits
        /// - allows <algorithm> functions to be used
        ///
        ////////////////////////////////////////////////////////////////////////////////
        typedef std::forward_iterator_tag  iterator_category;
        typedef ntl::ComBstr                  value_type;
        typedef int                        difference_type;
        typedef ntl::ComBstr*                 pointer;
        typedef ntl::ComBstr&                 reference;


    private:
        void increment()
        {
            if (dwIndex == END_ITERATOR_INDEX) {
                throw std::out_of_range("WmiClassObject::property_iterator - cannot increment: at the end");
            }

            CIMTYPE next_cimtype;
            ntl::ComBstr next_name;
            ntl::ComVariant next_value;
            HRESULT hr = this->wbemClassObj->Next(
                0,
                next_name.get_addr_of(),
                next_value.get(),
                &next_cimtype,
                nullptr);
            switch (hr) {
                case WBEM_S_NO_ERROR: {
                    // update the instance members
                    ++dwIndex;
                    using std::swap;
                    swap(propName, next_name);
                    swap(propType, next_cimtype);
                    break;
                }
#pragma warning (suppress : 6221)
                // "Implicit cast between semantically different integer types:  comparing HRESULT to an integer.  Consider using SUCCEEDED or FAILED macros instead."
                // Directly comparing the HRESULT return to WBEM_S_NO_ERROR, even though WBEM did not properly define that constant as an HRESULT
                case WBEM_S_NO_MORE_DATA: {
                    // at the end...
                    dwIndex = END_ITERATOR_INDEX;
                    propName.reset();
                    propType = 0;
                    break;
                }

                default:
                    throw ntl::WmiException(
                        hr, 
                        this->wbemClassObj.get(), 
                        L"IEnumWbemClassObject::Next", 
                        L"WmiClassObject::property_iterator::increment", 
                        false);
            }
        }
    };
};

} // namespace ntl
