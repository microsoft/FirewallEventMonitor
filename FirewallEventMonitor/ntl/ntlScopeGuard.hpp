// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

///
/// A modern scope guard originally developed by Stephan T. Lavavej, Visual C++ Libraries Developer 
/// - a couple of interface changes later made for this project
///
/// This template class facilitates building exception safe code by capturing state-restoration in a stack object
/// - which will be guaranteed to be run at scope exit (either by the code block executing through the end or by exception)
///
/// Example with calling member functions of instantiated objects (imagine friends is an stl container).
/// - Notice that in this example, we are reverting the vector to the prior state only if AddFriend fails.
///
///  void User::AddFriend(User& newFriend)
///  {
///      friends.push_back(newFriend);
/// 
///      auto popOnError( [&friends] () {friends.pop_back()}; );
///      ntl::ScopeGuard<decltype(popOnError)> guard(popOnError);
///
///      // imagine some database object instance 'db'
///      db->AddFriend(newFriend);
///      guard.dismiss();
///  }
///


// cpp headers
#include <memory> // for std::addressof()
// ntl headers
#include "ntlVersionConversion.hpp"


namespace ntl {

    template <typename F> class ScopeGuardT {
    public:
        explicit ScopeGuardT(F& f) NOEXCEPT :
            m_p(std::addressof(f))
        {
        }

        void run_once( ) NOEXCEPT
        {
            if (m_p) {
                (*m_p)();
            }
            m_p = nullptr;
        }

        void dismiss() NOEXCEPT
        {
            m_p = nullptr;
        }

        ~ScopeGuardT() NOEXCEPT
        {
            if (m_p) {
                (*m_p)();
            }
        }

        explicit ScopeGuardT(F&&) = delete;
        ScopeGuardT(const ScopeGuardT&) = delete;
        ScopeGuardT& operator=(const ScopeGuardT&) = delete;

    private:
        F * m_p;
    };

#define ScopeGuard(NAME, BODY)  \
    auto xx##NAME##xx = [&]() BODY; \
    ::ntl::ScopeGuardT<decltype(xx##NAME##xx)> NAME(xx##NAME##xx)

} // namespace
