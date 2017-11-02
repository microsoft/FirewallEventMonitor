// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <iterator>
#include <exception>
#include <stdexcept>
#include <memory>
// os headers
#include <windows.h>
#include <Wbemidl.h>
// local headers
#include "ntlComInitialize.hpp"
#include "ntlWmiService.hpp"
#include "ntlWmiInstance.hpp"
#include "ntlWmiException.hpp"
#include "ntlVersionConversion.hpp"


namespace ntl
{

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// class WmiEnumerate
///
/// Exposes enumerating instances of a WMI Provider through an iterator interface.
///
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class WmiEnumerate
{
public:
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// class WmiEnumerate::iterator
    ///
    /// A forward iterator class type to enable forward-traversing instances of the queried
    ///  WMI provider
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    class iterator
    {
    public:

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// c'tor and d'tor
        /// - default c'tor is an 'end' iterator
        /// - c'tor can take a reference to the parent's WMI Enum interface (to traverse)
        ///
        ////////////////////////////////////////////////////////////////////////////////
        explicit iterator(const WmiService& _services) NOEXCEPT : 
            index(END_ITERATOR_INDEX),
            wbemServices(_services),
            wbemEnumerator(), 
            wmiInstance()
        {
        }
        iterator(const WmiService& _services, const ntl::ComPtr<IEnumWbemClassObject>& _wbemEnumerator) : 
            index(0), 
            wbemServices(_services),
            wbemEnumerator(_wbemEnumerator), 
            wmiInstance()
        {
            this->increment();
        }
        ~iterator() NOEXCEPT
        {
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// copy c'tor and copy assignment
        ///
        /// Note that both are no-fail/no-throw operations
        ///
        ////////////////////////////////////////////////////////////////////////////////
        iterator(const iterator& _i) NOEXCEPT : 
            index(_i.index),
            wbemServices(_i.wbemServices),
            wbemEnumerator(_i.wbemEnumerator),
            wmiInstance(_i.wmiInstance)
        {
        }

        iterator& operator =(const iterator& _i) NOEXCEPT
        {
            iterator copy(_i);
            this->swap(copy);
            return *this;
        }

        void swap(_Inout_ iterator& _i) NOEXCEPT
        {
            using std::swap;
            swap(this->index, _i.index);
            swap(this->wbemServices, _i.wbemServices);
            swap(this->wbemEnumerator, _i.wbemEnumerator);
            swap(this->wmiInstance, _i.wmiInstance);
        }

        unsigned long location() const NOEXCEPT
        {
            return this->index;
        }
        
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// accessors:
        /// - dereference operators to access the internal WMI class object
        ///
        ////////////////////////////////////////////////////////////////////////////////
        WmiInstance& operator*() NOEXCEPT
        {
            return *this->wmiInstance;
        }
        WmiInstance* operator->() NOEXCEPT
        {
            return this->wmiInstance.get();
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
        bool operator==(const iterator&) const NOEXCEPT;
        bool operator!=(const iterator&) const NOEXCEPT;

        iterator& operator++(); // preincrement
        iterator  operator++(_In_ int); // postincrement
        iterator& operator+=(_In_ DWORD); // increment by integer

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// iterator_traits
        /// - allows <algorithm> functions to be used
        ///
        ////////////////////////////////////////////////////////////////////////////////
        typedef std::forward_iterator_tag  iterator_category;
        typedef WmiInstance              value_type;
        typedef int                        difference_type;
        typedef WmiInstance*             pointer;
        typedef WmiInstance&             reference;


    private:
        void increment();

        static const unsigned long END_ITERATOR_INDEX = 0xffffffff;

        unsigned long index;
        WmiService wbemServices;
        ntl::ComPtr<IEnumWbemClassObject> wbemEnumerator;
        std::shared_ptr<WmiInstance> wmiInstance;
    };


public:
    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// c'tor takes a reference to the initialized WmiService interface
    ///
    /// Default d'tor, copy c'tor, and copy assignment operators
    //
    ////////////////////////////////////////////////////////////////////////////////
    explicit WmiEnumerate(const WmiService& _wbemServices) NOEXCEPT :
        wbemServices(_wbemServices),
        wbemEnumerator()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// query(LPCWSTR)
    ///
    /// Allows for executing a WMI query against the WMI service for an enumeration
    /// of WMI objects.
    /// Assumes the query of of the WQL query language.
    ///
    /// Will throw a ntl::WmiException if the WMI query fails
    /// Will throw a std::bad_alloc if fails to low-resources
    ///
    ////////////////////////////////////////////////////////////////////////////////
    void query(_In_ LPCWSTR _query)
    {
        ntl::ComBstr wql(L"WQL");
        ntl::ComBstr query(_query);

        HRESULT hr = this->wbemServices->ExecQuery(
            wql.get(), 
            query.get(),
            WBEM_FLAG_BIDIRECTIONAL, 
            nullptr,
            this->wbemEnumerator.get_addr_of());
        if (FAILED(hr)) {
            throw ntl::WmiException(hr, L"IWbemServices::ExecQuery", L"WmiEnumerate::query", false);
        }
    }
    void query(_In_ LPCWSTR _query, const ntl::ComPtr<IWbemContext>& _context)
    {
        ntl::ComBstr wql(L"WQL");
        ntl::ComBstr query(_query);

        // forced to const-cast to make this const-correct as COM does not have
        //   an expression for saying a method call is const
        HRESULT hr = this->wbemServices->ExecQuery(
            wql.get(), 
            query.get(),
            WBEM_FLAG_BIDIRECTIONAL, 
            const_cast<IWbemContext*>(_context.get()),
            this->wbemEnumerator.get_addr_of());
        if (FAILED(hr)) {
            throw ntl::WmiException(hr, L"IWbemServices::ExecQuery", L"WmiEnumerate::query", false);
        }
    }


    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// access methods to the child iterator class
    ///
    /// begin() - iterator pointing to the first of the contained enumerator
    /// end()   - defined end-iterator for comparison
    ///
    /// cbegin() / cend() - const versions of the above
    ///
    /// Iterator construction can fail - will throw a ntl::WmiException
    ///
    ////////////////////////////////////////////////////////////////////////////////
    iterator begin() const
    {
        if (NULL == this->wbemEnumerator.get()) {
            return end();
        }

        HRESULT hr = this->wbemEnumerator->Reset();
        if (FAILED(hr)) {
            throw ntl::WmiException(hr, L"IEnumWbemClassObject::Reset", L"WmiEnumerate::begin", false);
        }
        
        return iterator(this->wbemServices, this->wbemEnumerator);
    }
    iterator end() const NOEXCEPT
    {
        return iterator(this->wbemServices);
    }
    const iterator cbegin() const
    {
        if (NULL == this->wbemEnumerator.get()) {
            return cend();
        }

        HRESULT hr = this->wbemEnumerator->Reset();
        if (FAILED(hr)) {
            throw ntl::WmiException(hr, L"IEnumWbemClassObject::Reset", L"WmiEnumerate::cbegin", false);
        }

        return iterator(this->wbemServices, this->wbemEnumerator);
    }
    const iterator cend() const NOEXCEPT
    {
        return iterator(this->wbemServices);
    }

private:
    WmiService wbemServices;
    //
    // Marking wbemEnumerator mutabale to allow for const correctness of begin() and end()
    //   specifically, invoking Reset() is an implementation detail and should not affect external contracts
    //
    mutable ntl::ComPtr<IEnumWbemClassObject> wbemEnumerator;
};


////////////////////////////////////////////////////////////////////////////////
///
/// Definitions of iterator comparison operators and arithmatic operators
///
////////////////////////////////////////////////////////////////////////////////
inline
bool WmiEnumerate::iterator::operator==(const iterator& _iter) const NOEXCEPT
{
    if (this->index != this->END_ITERATOR_INDEX) {
        return ( (this->index == _iter.index) && 
                 (this->wbemServices == _iter.wbemServices) &&
                 (this->wbemEnumerator == _iter.wbemEnumerator) &&
                 (this->wmiInstance == _iter.wmiInstance));
    } else {
        return ( (this->index == _iter.index) && 
                 (this->wbemServices == _iter.wbemServices));
    }
}
inline
bool WmiEnumerate::iterator::operator!=(const iterator& _iter) const NOEXCEPT
{
    return !(*this == _iter);
}
// preincrement
inline
WmiEnumerate::iterator& WmiEnumerate::iterator::operator++()
{
    this->increment();
    return *this;
}
// postincrement
inline
WmiEnumerate::iterator  WmiEnumerate::iterator::operator++(_In_ int)
{
    iterator temp (*this);
    this->increment();
    return temp;
}
// increment by integer
inline
WmiEnumerate::iterator& WmiEnumerate::iterator::operator+=(_In_ DWORD _inc)
{
    for (unsigned loop = 0; loop < _inc; ++loop) {
        this->increment();
        if (this->index == END_ITERATOR_INDEX) {
            throw std::out_of_range("WmiEnumerate::iterator::operator+= - invalid subscript");
        }
    }
    return *this;
}
inline
void WmiEnumerate::iterator::increment()
{
    if (index == END_ITERATOR_INDEX) {
        throw std::out_of_range("WmiEnumerate::iterator::increment at the end");
    }

    ULONG uReturn;
    ntl::ComPtr<IWbemClassObject> wbemTarget;
    HRESULT hr = this->wbemEnumerator->Next(
        WBEM_INFINITE,
        1,
        wbemTarget.get_addr_of(),
        &uReturn);
    if (FAILED(hr)) {
        throw ntl::WmiException(hr, L"IEnumWbemClassObject::Next", L"WmiEnumerate::iterator::increment", false);
    }

    if (0 == uReturn) {
        // at the end...
        this->index = END_ITERATOR_INDEX;
        this->wmiInstance.reset();
    } else {
        ++this->index;
        this->wmiInstance = std::make_shared<WmiInstance>(this->wbemServices, wbemTarget);
    }
}

} // namespace ntl
