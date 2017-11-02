// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <exception>
#include <vector>
#include <memory>
// os headers
#include <winsock2.h>
#include <ws2ipdef.h>
#include <Iphlpapi.h>
// ntl headers
#include "ntlVersionConversion.hpp"
#include "ntlException.hpp"
#include "ntlSockaddr.hpp"

namespace ntl {

    class NetAdapterAddresses {
    public:
        class iterator {
        public:
            ////////////////////////////////////////////////////////////////////////////////
            ///
            /// c'tor
            /// - NULL ptr is an 'end' iterator
            ///
            /// - default d'tor, copy c'tor, and copy assignment
            ///
            ////////////////////////////////////////////////////////////////////////////////
            iterator() = default;

            explicit iterator(_In_ std::shared_ptr<std::vector<BYTE>> _ipAdapter) NOEXCEPT : buffer(_ipAdapter)
            {
                if ((buffer.get() != nullptr) && (buffer->size() > 0)) {
                    current = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&(this->buffer->at(0)));
                }
            }

            ////////////////////////////////////////////////////////////////////////////////
            ///
            /// member swap method
            ///
            ////////////////////////////////////////////////////////////////////////////////
            void swap(_Inout_ iterator& _in) NOEXCEPT
            {
                using std::swap;
                swap(this->buffer, _in.buffer);
                swap(this->current, _in.current);
            }

            ////////////////////////////////////////////////////////////////////////////////
            ///
            /// accessors:
            /// - dereference operators to access the internal row
            ///
            ////////////////////////////////////////////////////////////////////////////////
            IP_ADAPTER_ADDRESSES& operator*()
            {
                if (this->current == nullptr) {
                    throw std::out_of_range("out_of_range: ntlNetAdapterAddresses::iterator::operator*");
                }
                return *(this->current);
            }
            IP_ADAPTER_ADDRESSES* operator->()
            {
                if (this->current == nullptr) {
                    throw std::out_of_range("out_of_range: ntlNetAdapterAddresses::iterator::operator->");
                }
                return this->current;
            }

            ////////////////////////////////////////////////////////////////////////////////
            ///
            /// comparison and arithmatic operators
            /// 
            /// comparison operators are no-throw/no-fail
            /// arithmatic operators can fail 
            ///
            ////////////////////////////////////////////////////////////////////////////////
            bool operator==(const iterator& _iter) const NOEXCEPT
            {
                // for comparison of 'end' iterators, just look at current
                if (this->current == nullptr) {
                    return (this->current == _iter.current);
                } else {
                    return ((this->buffer == _iter.buffer) &&
                            (this->current == _iter.current));
                }
            }
            bool operator!=(const iterator& _iter) const NOEXCEPT
            {
                return !(*this == _iter);
            }
            // preincrement
            iterator& operator++()
            {
                if (this->current == nullptr) {
                    throw std::out_of_range("out_of_range: ntlNetAdapterAddresses::iterator::operator++");
                }
                // increment
                current = current->Next;
                return *this;
            }
            // postincrement
            iterator  operator++(int)
            {
                iterator temp(*this);
                ++(*this);
                return temp;
            }
            // increment by integer
            iterator& operator+=(DWORD _inc)
            {
                for (unsigned loop = 0; (loop < _inc) && (this->current != nullptr); ++loop) {
                    current = current->Next;
                }
                if (this->current == nullptr) {
                    throw std::out_of_range("out_of_range: ntlNetAdapterAddresses::iterator::operator+=");
                }
                return *this;
            }

            ////////////////////////////////////////////////////////////////////////////////
            ///
            /// iterator_traits
            /// - allows <algorithm> functions to be used
            ///
            ////////////////////////////////////////////////////////////////////////////////
            typedef std::forward_iterator_tag   iterator_category;
            typedef IP_ADAPTER_ADDRESSES        value_type;
            typedef int                         difference_type;
            typedef IP_ADAPTER_ADDRESSES*       pointer;
            typedef IP_ADAPTER_ADDRESSES&       reference;

        private:
            std::shared_ptr<std::vector<BYTE>> buffer = nullptr;
            PIP_ADAPTER_ADDRESSES current = nullptr;
        };

    public:

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// c'tor
        ///
        /// - default d'tor, copy c'tor, and copy assignment
        /// - Takes an optional _gaaFlags argument which is passed through directly to
        ///   GetAdapterAddresses internally - use standard GAA_FLAG_* constants
        ///
        ////////////////////////////////////////////////////////////////////////////////
        explicit NetAdapterAddresses(unsigned _family = AF_UNSPEC, DWORD _gaaFlags = 0) : 
            buffer(new std::vector<BYTE>(16384))
        {
            this->refresh(_family, _gaaFlags);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// refresh
        ///
        /// - retrieves the current set of adapter address information
        /// - Takes an optional _gaaFlags argument which is passed through directly to
        ///   GetAdapterAddresses internally - use standard GAA_FLAG_* constants
        ///
        /// NOTE: this will invalidate any iterators from this instance
        /// NOTE: this only implements the Basic exception guarantee
        ///       if this fails, an exception is thrown, and any prior
        ///       information is lost. This is still safe to call after errors.
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void refresh(unsigned _family = AF_UNSPEC, DWORD _gaaFlags = 0)
        {
            // get both v4 and v6 adapter info
            ULONG byteSize = static_cast<ULONG>(this->buffer->size());
            ULONG err = ::GetAdaptersAddresses(
                _family,   // Family
                _gaaFlags, // Flags
                nullptr,   // Reserved
                reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&(this->buffer->at(0))),
                &byteSize
               );
            if (err == ERROR_BUFFER_OVERFLOW) {
                this->buffer->resize(byteSize);
                err = ::GetAdaptersAddresses(
                    _family,   // Family
                    _gaaFlags, // Flags
                    nullptr,   // Reserved
                    reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&(this->buffer->at(0))),
                    &byteSize
                   );
            }
            if (err != NO_ERROR) {
                throw ntl::Exception(err, L"GetAdaptersAddresses", L"ntlNetAdapterAddresses::ntNetAdapterAddresses", false);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// begin/end
        ///
        /// - constructs ntlNetAdapterAddresses::iterators
        ///
        ////////////////////////////////////////////////////////////////////////////////
        iterator begin() const NOEXCEPT
        {
            return iterator(this->buffer);
        }
        iterator end() const NOEXCEPT
        {
            return iterator();
        }

    private:
        ///
        /// private members
        ///
        std::shared_ptr<std::vector<BYTE>> buffer;
    };

    ///
    /// functor NetAdapterMatchingAddrPredicate
    ///
    /// Created to leverage STL algorigthms to parse a ntlNetAdapterAddresses set of iterators
    /// - to find the first interface that has the specified address assigned
    ///
    struct NetAdapterMatchingAddrPredicate {
        explicit NetAdapterMatchingAddrPredicate(const ntl::Sockaddr& _addr) : 
            targetAddr(_addr)
        {
        }

        bool operator () (const IP_ADAPTER_ADDRESSES& _ipAddress) NOEXCEPT
        {
            for (PIP_ADAPTER_UNICAST_ADDRESS unicastAddress = _ipAddress.FirstUnicastAddress;
                 unicastAddress != nullptr;
                 unicastAddress = unicastAddress->Next) 
            {
                ntl::Sockaddr unicastSockaddr(&unicastAddress->Address);
                if (unicastSockaddr == targetAddr) {
                    return true;
                }
            }
            return false;
        }

    private:
        ntl::Sockaddr targetAddr;
    };

} // namespace ntl

