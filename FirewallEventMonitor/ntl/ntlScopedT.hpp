// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <new>
#include <utility>
// ntl headers
#include "ntlVersionConversion.hpp"

namespace ntl {

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    //  template <typename T, T tNullValue, typename Fn> class ScopedT
    //
    //  - A template class which implements a "smart" resource class
    //    the resource of type T
    //
    //  - T tNullValue is a value of type T defining a known constant value that 
    //    does not need to be released
    //
    //  - typename Fn should reference a functor which implements:
    //            void operator()(T&) NOEXCEPT'
    //    - should free the resource type T
    //
    //  All methods are specified NOEXCEPT - none can throw
    //
    //  This class does not allow copy assignment or construction by design, but does
    //  allow move assignment and construction
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename T, T tNullValue, typename Fn>
    class ScopedT {
    public:
        ///////////////////////////////////////////////////////////////
        // constructors
        // - take a reference to T to take ownership
        // - optionally take a deleter Functor instance
        // - default constructor initializes with tNullValue
        ///////////////////////////////////////////////////////////////
        ScopedT() NOEXCEPT : tValue(tNullValue)
        {
        }
        // non-explicit by design
        explicit ScopedT(T const& t) NOEXCEPT : tValue(t)
        {
        }
        ScopedT(T const& t, Fn const& f) NOEXCEPT : tValue(t), closeFunctor(f)
        {
        }
        // allowing move construction (not copy construction)

        ScopedT(ScopedT const&) = delete;
        ScopedT& operator=(ScopedT const&) = delete;

        ScopedT(ScopedT&& other) NOEXCEPT : 
            tValue(std::move(other.tValue)),
            closeFunctor(std::move(other.closeFunctor))
        {
            // Stop the tValue from being destroyed as soon as other leaves scope
            other.tValue = tNullValue;
        }
        ScopedT& operator=(ScopedT&& other) NOEXCEPT
        {
            ScopedT(std::move(other)).swap(*this);
            return *this;
        }

        // default destructor (no-fail)
        ~ScopedT() NOEXCEPT
        {
            this->reset();
        }
        // getter
        const T& get() const NOEXCEPT
        {
            return this->tValue;
        }
        // function to free internal resources
        T release() NOEXCEPT
        {
            T value = this->tValue;
            this->tValue = tNullValue;
            return value;
        }
        void reset(T const& newValue = tNullValue) NOEXCEPT
        {
            closeFunctor(this->tValue);
            this->tValue = newValue;
        }
        // implementation of swap()
        void swap(ScopedT& tShared) NOEXCEPT
        {
            using std::swap;
            swap(this->closeFunctor, tShared.closeFunctor);
            swap(this->tValue, tShared.tValue);
        }

    private:
        T tValue;
        Fn closeFunctor;
    }; // class ScopedT

    ///////////////////////////////////////////////////////////////////
    // The non-member version of swap() within the ntl namespace
    //
    // no-throw guarantee
    ///////////////////////////////////////////////////////////////////
    template <typename T, T tNullValue, typename Fn>
    void swap(ScopedT<T, tNullValue, Fn>& a, ScopedT<T, tNullValue, Fn>& b) NOEXCEPT
    {
        a.swap(b);
    }

    ///////////////////////////////////////////////////////////////////
    // The non-member comparison operators within the ntl namespace
    //
    // no-throw guarantee
    ///////////////////////////////////////////////////////////////////
    template <typename T, T tNullValue, typename FnT, typename A, A aNullValue, typename FnA>
    bool operator==(ScopedT<T, tNullValue, FnT> const& lhs, ScopedT<A, aNullValue, FnA> const& rhs) NOEXCEPT
    {
        return (lhs.get() == rhs.get());
    }
    template <typename T, T tNullValue, typename FnT, typename A, A aNullValue, typename FnA>
    bool operator!=(ScopedT<T, tNullValue, FnT> const& lhs, ScopedT<A, aNullValue, FnA> const& rhs) NOEXCEPT
    {
        return (lhs.get() != rhs.get());
    }

}  // namespace ntl
