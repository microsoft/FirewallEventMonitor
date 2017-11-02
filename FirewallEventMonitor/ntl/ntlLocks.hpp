// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// os headers
#include <windows.h>
// ntl headers
#include "ntlVersionConversion.hpp"
#include "ntlException.hpp"

namespace ntl {

    ///
    /// Pure RAII tracking the state of this CS
    /// - notice there are no methods to explicitly control entering/leaving the CS
    ///
    class AutoReleaseCriticalSection {
    public:
        _Acquires_lock_(*this->cs)
        _Post_same_lock_(*_cs, *this->cs)
        explicit AutoReleaseCriticalSection(_In_ CRITICAL_SECTION* _cs) NOEXCEPT :
            cs(_cs)
        {
            ::EnterCriticalSection(this->cs);
        }

        _Releases_lock_(*this->cs)
        ~AutoReleaseCriticalSection() NOEXCEPT
        {
            ::LeaveCriticalSection(this->cs);
        }

        /// no default c'tor
        AutoReleaseCriticalSection() = delete;
        /// non-copyable
        AutoReleaseCriticalSection(const AutoReleaseCriticalSection&) = delete;
        AutoReleaseCriticalSection operator=(const AutoReleaseCriticalSection&) = delete;

    private:
        CRITICAL_SECTION* cs = nullptr;
    };


    class PrioritizedCriticalSection {
    public:
        PrioritizedCriticalSection() NOEXCEPT
        {
            ::InitializeSRWLock(&srwlock);
            if (!::InitializeCriticalSectionEx(&cs, 4000, 0)) {
                AlwaysFatalCondition(
                    L"PrioritizedCriticalSection: InitializeCriticalSectionEx failed [%u]",
                    ::GetLastError());
            }
        }
        ~PrioritizedCriticalSection() NOEXCEPT
        {
            ::DeleteCriticalSection(&cs);
        }

        // taking an *exclusive* lock to interrupt the *shared* lock taken by the deque IO path
        // - we want to interrupt the IO path so we can initiate more IO if we need to grow the CQ
        _Acquires_exclusive_lock_(this->srwlock)
        _Acquires_lock_(this->cs)
        void priority_lock() NOEXCEPT
        {
            ::AcquireSRWLockExclusive(&srwlock);
            ::EnterCriticalSection(&cs);
        }
        _Releases_lock_(this->cs)
        _Releases_exclusive_lock_(this->srwlock)
        void priority_release() NOEXCEPT
        {
            ::LeaveCriticalSection(&cs);
            ::ReleaseSRWLockExclusive(&srwlock);
        }

        _Acquires_shared_lock_(this->srwlock)
        _Acquires_lock_(this->cs)
        void default_lock() NOEXCEPT
        {
            ::AcquireSRWLockShared(&srwlock);
            ::EnterCriticalSection(&cs);
        }
        _Releases_lock_(this->cs)
        _Releases_shared_lock_(this->srwlock)
        void default_release() NOEXCEPT
        {
            ::LeaveCriticalSection(&cs);
            ::ReleaseSRWLockShared(&srwlock);
        }

        /// not copyable
        PrioritizedCriticalSection(const PrioritizedCriticalSection&) = delete;
        PrioritizedCriticalSection& operator=(const PrioritizedCriticalSection&) = delete;

    private:
        SRWLOCK srwlock;
        CRITICAL_SECTION cs;
    };
    class AutoReleasePriorityCriticalSection {
    public:
        explicit AutoReleasePriorityCriticalSection(PrioritizedCriticalSection& _priority_cs) NOEXCEPT :
            prioritized_cs(_priority_cs)
        {
            prioritized_cs.priority_lock();
        }

        ~AutoReleasePriorityCriticalSection() NOEXCEPT
        {
            prioritized_cs.priority_release();
        }

        /// no default c'tor
        AutoReleasePriorityCriticalSection() = delete;
        /// non-copyable
        AutoReleasePriorityCriticalSection(const AutoReleasePriorityCriticalSection&) = delete;
        AutoReleasePriorityCriticalSection operator=(const AutoReleasePriorityCriticalSection&) = delete;

    private:
        PrioritizedCriticalSection& prioritized_cs;
    };
    class AutoReleaseDefaultCriticalSection {
    public:
        explicit AutoReleaseDefaultCriticalSection(PrioritizedCriticalSection& _priority_cs) NOEXCEPT :
            prioritized_cs(_priority_cs)
        {
            prioritized_cs.default_lock();
        }

        ~AutoReleaseDefaultCriticalSection() NOEXCEPT
        {
            prioritized_cs.default_release();
        }

        /// no default c'tor
        AutoReleaseDefaultCriticalSection() = delete;
        /// non-copyable
        AutoReleaseDefaultCriticalSection(const AutoReleaseDefaultCriticalSection&) = delete;
        AutoReleaseDefaultCriticalSection operator=(const AutoReleaseDefaultCriticalSection&) = delete;

    private:
        PrioritizedCriticalSection& prioritized_cs;
    };

    //////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// Can concurrent-safely read from both const and non-const
    ///  long long * 
    ///  long *
    ///
    //////////////////////////////////////////////////////////////////////////////////////////
    inline long long MemoryGuardRead(const long long* _original_value) NOEXCEPT
    {
        return ::InterlockedCompareExchange64(const_cast<volatile long long*>(_original_value), 0LL, 0LL);
    }
    inline long MemoryGuardRead(const long* _original_value) NOEXCEPT
    {
        return ::InterlockedCompareExchange(const_cast<volatile long*>(_original_value), 0LL, 0LL);
    }
    inline long long MemoryGuardRead(_In_ long long* _original_value) NOEXCEPT
    {
        return ::InterlockedCompareExchange64(const_cast<volatile long long*>(_original_value), 0LL, 0LL);
    }
    inline long MemoryGuardRead(_In_ long* _original_value) NOEXCEPT
    {
        return ::InterlockedCompareExchange(const_cast<volatile long*>(_original_value), 0LL, 0LL);
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// Can concurrent-safely update a long long or long value
    /// - *Write returns the *prior* value
    /// - *WriteConditionally returns the *prior* value
    /// - *Add returns the *prior* value
    /// - *Subtract returns the *prior* value
    ///   (Note subtraction is just the adding a negative long value)
    ///
    /// - *Increment returns the *new* value
    /// - *Decrement returns the *new* value
    ///
    //////////////////////////////////////////////////////////////////////////////////////////
    inline long long MemoryGuardWrite(_Inout_ long long* _original_value, long long _new_value) NOEXCEPT
    {
        return ::InterlockedExchange64(_original_value, _new_value);
    }
    inline long MemoryGuardWrite(_Inout_ long* _original_value, long _new_value) NOEXCEPT
    {
        return ::InterlockedExchange(_original_value, _new_value);
    }

    inline long long MemoryGuardWriteConditionally(_Inout_ long long* _original_value, long long _new_value, long long _if_equals) NOEXCEPT
    {
        return ::InterlockedCompareExchange64(_original_value, _new_value, _if_equals);
    }
    inline long MemoryGuardWriteConditionally(_Inout_ long* _original_value, long _new_value, long _if_equals) NOEXCEPT
    {
        return ::InterlockedCompareExchange(_original_value, _new_value, _if_equals);
    }

    inline long long MemoryGuardAdd(_Inout_ long long* _original_value, long long _add_value) NOEXCEPT
    {
        return ::InterlockedExchangeAdd64(_original_value, _add_value);
    }
    inline long MemoryGuardAdd(_Inout_ long* _original_value, long _add_value) NOEXCEPT
    {
        return ::InterlockedExchangeAdd(_original_value, _add_value);
    }

    inline long long MemoryGuardSubtract(_Inout_ long long* _original_value, long long _subtract_value) NOEXCEPT
    {
        return ::InterlockedExchangeAdd64(_original_value, _subtract_value * -1LL);
    }
    inline long MemoryGuardSubtract(_Inout_ long* _original_value, long _subtract_value) NOEXCEPT
    {
        return ::InterlockedExchangeAdd(_original_value, _subtract_value * -1L);
    }

    inline long long MemoryGuardIncrement(_Inout_ long long* _original_value) NOEXCEPT
    {
        return ::InterlockedIncrement64(_original_value);
    }
    inline long MemoryGuardIncrement(_Inout_ long* _original_value) NOEXCEPT
    {
        return ::InterlockedIncrement(_original_value);
    }

    inline long long MemoryGuardDecrement(_Inout_ long long* _original_value) NOEXCEPT
    {
        return ::InterlockedDecrement64(_original_value);
    }
    inline long MemoryGuardDecrement(_Inout_ long* _original_value) NOEXCEPT
    {
        return ::InterlockedDecrement(_original_value);
    }

} // namespace
