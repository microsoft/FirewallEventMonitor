// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <utility>
#include <string>
#include <vector>

// os headers
#include <Windows.h>
#include <Objbase.h>
#include <OleAuto.h>

// local headers
#include "ntlException.hpp"
#include "ntlVersionConversion.hpp"
#include "ntlScopeGuard.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// This header exposes the following interfaces - designed to make use of COM and its resources
///  both exception-safe and enabling RAII design patterns.
///
/// All the below *except ComInitialize* have the following traits:
/// - all are copyable
/// - all implement get()/set()
/// - all implement swap()
///
/// class ComInitialize
/// class ComBstr
/// class ComVariant
/// template <typename T> class ComPtr
///
///
/// Compilation units including this header must link against the following libraries:
/// - ole32.lib
/// - oleaut32.lib
///
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ntl
{

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// Callers are expected to have a ComInitialize instance alive on every thread
    ///   they use COM and WMI. The Com* and Wmi* classes do not call this from
    ///   within the library code.
    ///
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    class ComInitialize {
    public:
        ///////////////////////////////////////////////////////////////////////////////////////////////
        ///
        /// ntl classes have no requirement to be explicitly COINIT_APARTMENTTHREADED
        /// - thus defaulting to COINIT_MULTITHREADED as they can be used with either
        ///
        ///////////////////////////////////////////////////////////////////////////////////////////////
        explicit ComInitialize(DWORD _threading_model = COINIT_MULTITHREADED)
        {
            HRESULT hr = ::CoInitializeEx(nullptr, _threading_model);
            switch (hr) {
            case S_OK:
            case S_FALSE:
                uninit_required = true;
                break;
            case RPC_E_CHANGED_MODE:
                uninit_required = false;
                break;
            default:
                throw ntl::Exception(hr, L"CoInitializeEx", L"ComInitialize::ComInitialize", false);
            }
        }
        ~ComInitialize() NOEXCEPT
        {
            if (uninit_required) {
                ::CoUninitialize();
            }
        }

        ComInitialize(const ComInitialize&) = delete;
        ComInitialize& operator =(const ComInitialize&) = delete;
        ComInitialize(ComInitialize&&) = delete;
        ComInitialize& operator =(ComInitialize&&) = delete;

    private:
        bool uninit_required = false;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// template<typename T>
    /// class ComPtr
    ///
    /// Template class tracking the lifetime of a COM interface pointer.
    ///
    /// Tracks the COM initialization - guaranteeing uninit when goes out of scope
    ///
    /// TODO: add  create() method to allow CoCreateInstance without forcing the static method
    ///       and therefor not requiring the user to specify an RIID or CLSID
    ///          riid = __uuidof(T);
    ///          LPCOLESTR clsid = "CLSID_" + (StringFromIid(riid)+1);
    ///          CoCreateInstance(clsid, ..., riid, ...)
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename T>
    class ComPtr {
    public:
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// createInstance(REFCLSID, REFIID)
        ///
        /// Static utility function to CoCreate the tempate interface type
        /// - exists as a useful factory to construct COM objects
        ///
        /// Will throw a ntl::Exception on failure
        ///
        ////////////////////////////////////////////////////////////////////////////////
        static ComPtr createInstance(REFCLSID _clsid, REFIID _riid)
        {
            ComPtr temp;
            HRESULT hr = ::CoCreateInstance(
                _clsid,
                nullptr,
                CLSCTX_INPROC_SERVER,
                _riid,
                reinterpret_cast<LPVOID*>(&temp.t));
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"CoCreateInstance", L"ComPtr::createInstance", false);
            }
            return temp;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// c'tor and d'tor
        ///
        /// The caller *should* Release() after assigning into this object to track.
        ///  This c'tor is designed to make refcounting less confusing to the caller
        ///  (they should always match their addref's with their releases)
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ComPtr() = default;
        explicit ComPtr(_In_opt_ T* _t) NOEXCEPT : t(_t)
        {
            if (t != nullptr) {
                t->AddRef();
            }
        }

        ~ComPtr() NOEXCEPT
        {
            release();
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// copy c'tor and copy assignment
        ///
        /// All are no-throw/no-fail operations
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ComPtr(const ComPtr& _obj) NOEXCEPT : t(_obj.t)
        {
            if (t != nullptr) {
                t->AddRef();
            }
        }
        ComPtr& operator =(const ComPtr& _obj) NOEXCEPT
        {
            ComPtr copy(_obj);
            this->swap(copy);
            return *this;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// move c'tor and move assignment
        ///
        /// All are no-throw/no-fail operations
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ComPtr(_In_ ComPtr&& _obj) NOEXCEPT
        {
            // initialized to nullptr ... swap with the [in] object
            this->swap(_obj);
        }
        ComPtr& operator =(_In_ ComPtr&& _obj) NOEXCEPT
        {
            ComPtr temp(std::move(_obj));
            this->swap(temp);
            return *this;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// comparison operators
        ///
        /// Boolean comparison on the pointer values
        ///
        /// All are no-throw/no-fail operations
        ///
        ////////////////////////////////////////////////////////////////////////////////
        bool operator ==(const ComPtr& _obj) const NOEXCEPT
        {
            return t == _obj.t;
        }
        bool operator !=(const ComPtr& _obj) const NOEXCEPT
        {
            return t != _obj.t;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// void set(T*)
        ///
        /// An explicit assignment method.
        ///
        /// The caller *should* Release() after assigning into this object to track.
        ///  This c'tor isn't designed to make refcounting more confusing to the caller
        ///  (they should always match their addref's with their releases)
        ///
        /// A no-throw/no-fail operation
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void set(const T* _ptr) NOEXCEPT
        {
            release();
            t = _ptr;
            if (t) {
                t->AddRef();
            }
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// accessors
        ///
        /// T* operator ->()
        /// - This getter allows the caller to dereference the object as if they were
        ///   directly calling methods off of the encapsulated object.
        ///
        /// T* get()
        /// - This getter directly retrieves the raw interface pointer value.
        ///
        /// IUnknown* get_IUnknown()
        /// - This getter retrieves an IUnknown* from the encapsulated COM ptr.
        ///   Note that all COM ptrs derive from IUnknown.
        ///   This is provided for type-safety and clarity.
        /// 
        /// T** get_addr_of()
        /// - This directly exposes the address of the encapsulated interface pointer
        ///   This break of encapsulation is required as COM will return interface
        ///    pointer values as [out] parameters.
        ///   Note this is *not* const to allow it to be used as an [out] param.
        ///
        /// All are no-throw/no-fail operations
        ///
        ////////////////////////////////////////////////////////////////////////////////
        T* operator ->() NOEXCEPT
        {
            return t;
        }
        const T* operator ->() const NOEXCEPT
        {
            return t;
        }
        T* get() NOEXCEPT
        {
            return t;
        }
        const T* get() const NOEXCEPT
        {
            return t;
        }
        T** get_addr_of() NOEXCEPT
        {
            release();
            return (&t);
        }
        IUnknown* get_IUnknown() NOEXCEPT
        {
            return t;
        }
        const IUnknown* get_IUnknown() const NOEXCEPT
        {
            return t;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// release()
        ///
        /// Explicitly releases a refcount of the encapsulated interface ptr
        /// Note that once released, the object no longer tracks that interface pointer
        ///
        /// A no-throw/no-fail operation
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void release() NOEXCEPT
        {
            if (t != nullptr) {
                t->Release();
            }
            t = nullptr;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// swap(ComPtr&)
        ///
        /// A no-fail swap() operator to safely swap the internal values of 2 objects
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void swap(_Inout_ ComPtr& _in) NOEXCEPT
        {
            using std::swap;
            swap(t, _in.t);
        }

    private:
        T* t = nullptr;
    };

    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// non-member swap()
    ///
    /// Required so the correct swap() function is called if the caller writes:
    ///  (which can be done in STL algorithms as well).
    ///
    /// swap(com1, com2);
    ///
    ////////////////////////////////////////////////////////////////////////////////
    template <typename T>
    inline void swap(_Inout_ ntl::ComPtr<T>& _lhs, _Inout_ ntl::ComPtr<T>& _rhs) NOEXCEPT
    {
        _lhs.swap(_rhs);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// class ComBstr
    ///
    /// Encapsulates a BSTR value, guaranteeing resource management and exception safety
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    class ComBstr {
    public:
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// c'tor and d'tor
        ///
        /// The c'tor can take a raw string ptr which it copies to the BSTR.
        /// - on failure, will throw std::bad_alloc (derived from std::exception)
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ComBstr() NOEXCEPT : bstr(nullptr)
        {
        }

        explicit ComBstr(_In_opt_ LPCWSTR _string) : bstr(nullptr)
        {
            if (_string != nullptr) {
                bstr = ::SysAllocString(_string);
                if (nullptr == bstr) {
                    throw std::bad_alloc();
                }
            }
        }

        explicit ComBstr(_In_opt_ const BSTR _string) : bstr(nullptr)
        {
            if (_string != nullptr) {
                bstr = ::SysAllocString(_string);
                if (nullptr == bstr) {
                    throw std::bad_alloc();
                }
            }
        }

        ComBstr(_In_reads_z_(_len) LPCWSTR _string, _In_ size_t _len) : bstr(nullptr)
        {
            if (_string != nullptr) {
                bstr = ::SysAllocStringLen(_string, static_cast<UINT>(_len));
                if (nullptr == bstr) {
                    throw std::bad_alloc();
                }
            }
        }

        ~ComBstr() NOEXCEPT
        {
            ::SysFreeString(bstr);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// copy c'tor and copy assignment
        ///
        /// Note that both can fail under low-resources
        /// - will throw std::bad_alloc (derived from std::exception)
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ComBstr(const ComBstr& _copy) : bstr(nullptr)
        {
            if (_copy.bstr != nullptr) {
                bstr = ::SysAllocString(_copy.bstr);
                if (nullptr == bstr) {
                    throw std::bad_alloc();
                }
            }
        }
        ComBstr& operator =(const ComBstr& _copy)
        {
            ComBstr temp(_copy);
            this->swap(temp);
            return *this;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// move c'tor and move assignment
        ///
        /// Both are no-fail
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ComBstr(_In_ ComBstr&& _obj) NOEXCEPT : bstr(nullptr)
        {
            this->swap(_obj);
        }
        ComBstr& operator =(_In_ ComBstr&& _obj) NOEXCEPT
        {
            ComBstr temp(std::move(_obj));
            this->swap(temp);
            return *this;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// size() and resize()
        ///
        /// retrieve and modify the internal size of the buffer containing the string
        ///
        ////////////////////////////////////////////////////////////////////////////////
        size_t size() const NOEXCEPT
        {
            // if bstr is nullptr, will return zero
            return ::SysStringLen(bstr);
        }
        void resize(const size_t string_length)
        {
            if (bstr != nullptr) {
                if (!::SysReAllocStringLen(&bstr, nullptr, static_cast<unsigned int>(string_length))) {
                    throw std::bad_alloc();
                }
            } else {
                bstr = ::SysAllocStringLen(nullptr, static_cast<unsigned int>(string_length));
                if (nullptr == bstr) {
                    throw std::bad_alloc();
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// reset(), set(), get()
        ///
        /// explicit BSTR access methods
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void reset() NOEXCEPT
        {
            ::SysFreeString(bstr);
            bstr = nullptr;
        }
        void set(_In_opt_ LPCWSTR _string)
        {
            ComBstr temp(_string);
            this->swap(temp);
        }
        void set(_In_opt_ const BSTR _bstr)
        {
            ComBstr temp(_bstr);
            this->swap(temp);
        }
        BSTR get() NOEXCEPT
        {
            return bstr;
        }
        const BSTR get() const NOEXCEPT
        {
            return bstr;
        }
        BSTR* get_addr_of() NOEXCEPT
        {
            ::SysFreeString(bstr);
            bstr = nullptr;
            return &bstr;
        }
        LPCWSTR c_str() const NOEXCEPT
        {
            return static_cast<LPCWSTR>(bstr);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// swap(ComBstr&)
        ///
        /// A no-fail swap() operator to safely swap the internal values of 2 objects
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void swap(_Inout_ ComBstr& _in) NOEXCEPT
        {
            using std::swap;
            swap(bstr, _in.bstr);
        }

    private:
        BSTR bstr;
    };
    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// non-member swap()
    ///
    /// Required so the correct swap() function is called if the caller writes:
    ///  (which can be done in STL algorithms as well).
    ///
    /// swap(bstr1, bstr2);
    ///
    ////////////////////////////////////////////////////////////////////////////////
    inline void swap(_Inout_ ntl::ComBstr& _lhs, _Inout_ ntl::ComBstr& _rhs) NOEXCEPT
    {
        _lhs.swap(_rhs);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// class ComVariant
    ///
    /// Encapsulates a VARIANT value, guaranteeing resource management and exception safety
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    class ComVariant {
    public:
        ///////////////////////////////////////////////////////////////////////////////////////////////
        ///
        /// VarTypeConverter
        ///
        /// struct to facilitate establishing a type based off a variant type
        /// - allows for template functions based off of a vartype to accept the correct type
        ///
        ///////////////////////////////////////////////////////////////////////////////////////////////
        template <VARTYPE VT>
        struct VarTypeConverter {};

        template <>
        struct VarTypeConverter<VT_I1> {
            typedef signed char assign_type;
            typedef signed char return_type;
        };
        template <>
        struct VarTypeConverter<VT_UI1> {
            typedef unsigned char assign_type;
            typedef unsigned char return_type;
        };
        template <>
        struct VarTypeConverter<VT_I2> {
            typedef signed short assign_type;
            typedef signed short return_type;
        };
        template <>
        struct VarTypeConverter<VT_UI2> {
            typedef unsigned short assign_type;
            typedef unsigned short return_type;
        };
        template <>
        struct VarTypeConverter<VT_I4> {
            typedef signed long assign_type;
            typedef signed long return_type;
        };
        template <>
        struct VarTypeConverter<VT_UI4> {
            typedef unsigned long assign_type;
            typedef unsigned long return_type;
        };
        template <>
        struct VarTypeConverter<VT_INT> {
            typedef signed int assign_type;
            typedef signed int return_type;
        };
        template <>
        struct VarTypeConverter<VT_UINT> {
            typedef unsigned int assign_type;
            typedef unsigned int return_type;
        };
        template <>
        struct VarTypeConverter<VT_I8> {
            typedef signed long long assign_type;
            typedef signed long long return_type;
        };
        template <>
        struct VarTypeConverter<VT_UI8> {
            typedef unsigned long long assign_type;
            typedef unsigned long long return_type;
        };
        template <>
        struct VarTypeConverter<VT_R4> {
            typedef float assign_type;
            typedef float return_type;
        };
        template <>
        struct VarTypeConverter<VT_R8> {
            typedef double assign_type;
            typedef double return_type;
        };
        template <>
        struct VarTypeConverter<VT_BOOL> {
            typedef bool assign_type;
            typedef bool return_type;
        };
        template <>
        struct VarTypeConverter<VT_BSTR> {
            // Defaulting the VT_BSTR type to LPCWSTR instead of type BSTR
            //   so the user can pass either an LPCWSTR or a BSTR through
            // - a BSTR can pass to a function taking a LPCWSTR
            // - a LPCWSTR can *not* pass to a function taking a BSTR
            typedef LPCWSTR   assign_type;
            typedef ntl::ComBstr return_type;
        };
        template <>
        struct VarTypeConverter<VT_DATE> {
            typedef SYSTEMTIME assign_type;
            typedef SYSTEMTIME return_type;
        };
        template <>
        struct VarTypeConverter<VT_BSTR | VT_ARRAY> {
            typedef const std::vector<std::wstring>& assign_type;
            typedef std::vector<std::wstring>        return_type;
        };
        template <>
        struct VarTypeConverter<VT_UI4 | VT_ARRAY> {
            typedef const std::vector<unsigned long>& assign_type;
            typedef std::vector<unsigned long>        return_type;
        };
        template <>
        struct VarTypeConverter<VT_UI2 | VT_ARRAY> {
            typedef const std::vector<unsigned short>& assign_type;
            typedef std::vector<unsigned short>        return_type;
        };
        template <>
        struct VarTypeConverter<VT_UI1 | VT_ARRAY> {
            typedef const std::vector<unsigned char>& assign_type;
            typedef std::vector<unsigned char>        return_type;
        };

    public:
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// c'tor and d'tor
        /// 
        /// Guarantees initialization and cleanup of the encapsulated VARIANT
        /// The template constructor directly constructs a VARIANT based off the
        ///  input type, streamlining coding requirements from the caller.
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ComVariant() NOEXCEPT
        {
            ::VariantInit(&variant);
        }

        explicit ComVariant(const VARIANT* _vt) : variant()
        {
            ::VariantInit(&variant);
            HRESULT hr = ::VariantCopy(&variant, _vt);
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"VariantCopy", L"ComVariant::ComVariant", false);
            }
        }

        ~ComVariant() NOEXCEPT
        {
            ::VariantClear(&variant);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// copy c'tor and copy assignment
        ///
        /// Note that both can fail under low-resources
        /// - will throw std::bad_alloc (derived from std::exception)
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ComVariant(const ComVariant& _copy) : variant()
        {
            ::VariantInit(&variant);
            HRESULT hr = ::VariantCopy(&variant, &_copy.variant);
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"VariantCopy", L"ComVariant::ComVariant", false);
            }
        }
        ComVariant& operator =(const ComVariant& _copy)
        {
            ComVariant temp(_copy);
            this->swap(temp);
            return *this;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// move c'tor and move assignment
        ///
        /// both are no-fail
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ComVariant(_In_ ComVariant&& _copy) : variant()
        {
            ::VariantInit(&variant);
            this->swap(_copy);
        }
        ComVariant& operator =(_In_ ComVariant&& _copy)
        {
            ComVariant temp(std::move(_copy));
            this->swap(temp);
            return *this;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// operator ->, reset(), set(), get()
        ///
        /// explicit VARIANT access methods
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void reset() NOEXCEPT
        {
            ::VariantClear(&variant);
            // reinitialize in case someone wants to immediately reuse
            ::VariantInit(&variant);
        }
        void set(const VARIANT* _vt)
        {
            ComVariant temp(_vt);
            this->swap(temp);
        }
        VARIANT* operator->() NOEXCEPT
        {
            return (&variant);
        }
        const VARIANT* operator->() const NOEXCEPT
        {
            return (&variant);
        }
        VARIANT* get() NOEXCEPT
        {
            return (&variant);
        }
        const VARIANT* get() const NOEXCEPT
        {
            return (&variant);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// accessors to query for / modify nullptr and EMPTY variant types
        ///
        /// all are const no-throw
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void set_empty() NOEXCEPT
        {
            reset();
            variant.vt = VT_EMPTY;
        }
        void set_null() NOEXCEPT
        {
            reset();
            variant.vt = VT_NULL;
        }
        bool is_empty() const NOEXCEPT
        {
            return (variant.vt == VT_EMPTY);
        }
        bool is_null() const NOEXCEPT
        {
            return (variant.vt == VT_NULL);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// swap(ComVariant&)
        ///
        /// A no-fail swap() operator to safely swap the internal values of 2 objects
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void swap(_Inout_ ComVariant& _in) NOEXCEPT
        {
            using std::swap;
            swap(variant, _in.variant);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// comparison operators
        ///
        ////////////////////////////////////////////////////////////////////////////////
        bool operator ==(const ComVariant& _in) const NOEXCEPT
        {
            if (variant.vt == VT_NULL) {
                return (_in.variant.vt == VT_NULL);
            }

            if (variant.vt == VT_EMPTY) {
                return (_in.variant.vt == VT_EMPTY);
            }

            if (variant.vt == VT_BSTR) {
                if (_in.variant.vt == VT_BSTR) {
                    return (0 == _wcsicmp(variant.bstrVal, _in.variant.bstrVal));
                } else {
                    return false;
                }
            }

            if (variant.vt == VT_DATE) {
                if (_in.variant.vt == VT_DATE) {
                    return (variant.date == _in.variant.date);
                } else {
                    return false;
                }
            }

            //
            // intentionally not supporting comparing floating-point types
            // - it's not going to provide a correct value
            // - the proper comparison should be < or  >
            //
            if ((variant.vt == VT_R4) || (_in.variant.vt == VT_R4) ||
                (variant.vt == VT_R8) || (_in.variant.vt == VT_R8)) {
                AlwaysFatalCondition(L"Not making equality comparisons on floating-point numbers");
            }
            //
            // Comparing integer types - not tightly enforcing type by default
            // - except for VT_BOOL
            // - maintaining that logical BOOLEAN comparison
            //
            // integer values to compare
            // - left hand side ('this' value)
            // - right hand side ('_in' value)
            //
            unsigned lhs, rhs;
            switch (variant.vt) {
            case VT_BOOL:
                lhs = static_cast<unsigned>(variant.boolVal);
                break;
            case VT_I1:
                lhs = static_cast<unsigned>(variant.cVal);
                break;
            case VT_UI1:
                lhs = static_cast<unsigned>(variant.bVal);
                break;
            case VT_I2:
                lhs = static_cast<unsigned>(variant.iVal);
                break;
            case VT_UI2:
                lhs = static_cast<unsigned>(variant.uiVal);
                break;
            case VT_I4:
                lhs = static_cast<unsigned>(variant.lVal);
                break;
            case VT_UI4:
                lhs = static_cast<unsigned>(variant.ulVal);
                break;
            case VT_INT:
                lhs = static_cast<unsigned>(variant.intVal);
                break;
            case VT_UINT:
                lhs = static_cast<unsigned>(variant.uintVal);
                break;
            default:
                return false;
            }
            switch (_in.variant.vt) {
            case VT_BOOL:
                rhs = static_cast<unsigned>(_in.variant.boolVal);
                break;
            case VT_I1:
                rhs = static_cast<unsigned>(_in.variant.cVal);
                break;
            case VT_UI1:
                rhs = static_cast<unsigned>(_in.variant.bVal);
                break;
            case VT_I2:
                rhs = static_cast<unsigned>(_in.variant.iVal);
                break;
            case VT_UI2:
                rhs = static_cast<unsigned>(_in.variant.uiVal);
                break;
            case VT_I4:
                rhs = static_cast<unsigned>(_in.variant.lVal);
                break;
            case VT_UI4:
                rhs = static_cast<unsigned>(_in.variant.ulVal);
                break;
            case VT_INT:
                rhs = static_cast<unsigned>(_in.variant.intVal);
                break;
            case VT_UINT:
                rhs = static_cast<unsigned>(_in.variant.uintVal);
                break;
            default:
                return false;
            }

            if (variant.vt == VT_BOOL) {
                return (variant.boolVal) ? (rhs != 0) : (rhs == 0);
            }

            if (_in.variant.vt == VT_BOOL) {
                return (_in.variant.boolVal) ? (lhs != 0) : (lhs == 0);
            }
            return (lhs == rhs);
        }

        bool operator !=(const ComVariant& _in) const NOEXCEPT
        {
            return !(*this == _in);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// write() method to print out the variant value
        /// - optional [in] parameter affects how to print if an integer-type
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ntl::ComBstr write(const bool _int_in_hex = false) const
        {
            static const unsigned IntegerLength = 32;
            ntl::ComBstr bstr;
            unsigned int_radix = (_int_in_hex) ? 16 : 10;

            switch (variant.vt) {
            case VT_EMPTY:
                bstr.set(L"<empty>");
                break;

            case VT_NULL:
                bstr.set(L"<null>");
                break;

            case VT_BOOL:
                bstr.set((variant.boolVal) ? L"true" : L"false");
                break;;

            case VT_I1:
                bstr.resize(IntegerLength);
                _itow_s(variant.cVal, bstr.get(), IntegerLength, int_radix);
                break;

            case VT_UI1:
                bstr.resize(IntegerLength);
                _itow_s(variant.bVal, bstr.get(), IntegerLength, int_radix);
                break;

            case VT_I2:
                bstr.resize(IntegerLength);
                _itow_s(variant.iVal, bstr.get(), IntegerLength, int_radix);
                break;

            case VT_UI2:
                bstr.resize(IntegerLength);
                _itow_s(variant.uiVal, bstr.get(), IntegerLength, int_radix);
                break;

            case VT_I4:
                bstr.resize(IntegerLength);
                _itow_s(variant.lVal, bstr.get(), IntegerLength, int_radix);
                break;

            case VT_UI4:
                bstr.resize(IntegerLength);
                _itow_s(variant.ulVal, bstr.get(), IntegerLength, int_radix);
                break;

            case VT_INT:
                bstr.resize(IntegerLength);
                _itow_s(variant.intVal, bstr.get(), IntegerLength, int_radix);
                break;

            case VT_UINT:
                bstr.resize(IntegerLength);
                _itow_s(variant.uintVal, bstr.get(), IntegerLength, int_radix);
                break;


            case VT_R4:
            {
                std::string float_str(_CVTBUFSIZE, 0x00);
                int err = _gcvt_s(&float_str[0], float_str.size(), variant.fltVal, 4); // up to 4 significant digits
                if (err != 0) {
                    throw ntl::Exception(err, L"_gcvt_s", L"ComVariantl::write", false);
                }
                float_str.resize(strlen(&float_str[0]));

                int len = ::MultiByteToWideChar(CP_UTF8, 0, float_str.c_str(), -1, nullptr, 0);
                if (len == 0) {
                    throw ntl::Exception(::GetLastError(), L"MultiByteToWideChar", L"ComVariantl::write", false);
                }

                bstr.resize(len);
                len = ::MultiByteToWideChar(CP_UTF8, 0, float_str.c_str(), -1, bstr.get(), len);
                if (len == 0) {
                    throw ntl::Exception(::GetLastError(), L"MultiByteToWideChar", L"ComVariantl::write", false);
                }
                break;
            }

            case VT_R8:
            {
                std::string float_str(_CVTBUFSIZE, 0x00);
                int err = _gcvt_s(&float_str[0], float_str.size(), variant.dblVal, 4); // up to 4 significant digits
                if (err != 0) {
                    throw ntl::Exception(err, L"_gcvt_s", L"ComVariantl::write", false);
                }
                float_str.resize(strlen(&float_str[0]));

                int len = ::MultiByteToWideChar(CP_UTF8, 0, float_str.c_str(), -1, nullptr, 0);
                if (len == 0) {
                    throw ntl::Exception(::GetLastError(), L"MultiByteToWideChar", L"ComVariantl::write", false);
                }

                bstr.resize(len);
                len = ::MultiByteToWideChar(CP_UTF8, 0, float_str.c_str(), -1, bstr.get(), len);
                if (len == 0) {
                    throw ntl::Exception(::GetLastError(), L"MultiByteToWideChar", L"ComVariantl::write", false);
                }
                break;
            }

            case VT_BSTR:
                bstr.set(variant.bstrVal);
                break;

            case VT_DATE:
            {
                SYSTEMTIME st;
                retrieve(&st);
                // write out: yyy-mm-dd HH:MM:SS:mmm
                // - based off of how CIM DATETIME is written per http://msdn.microsoft.com/en-us/library/aa387237(VS.85).aspx
                wchar_t sz_time[25];
                if (-1 == ::_snwprintf_s(
                    sz_time, 25, 24,
                    L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds
                   )) {
                    throw ntl::Exception(errno, L"_snwprintf_s VT_DATE conversion", L"ComVariantl::write", false);
                }
                bstr.set(sz_time);
                break;
            }


            default:
                throw ntl::Exception(variant.vt, L"Unknown VARAINT type", L"ComVariantl::write", false);
            }

            return bstr;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// Possible VARIANT types (VARTYPE)
        ///
        ///  VT_EMPTY            nothing
        ///  VT_NULL             SQL style Null
        ///  VT_I1               signed char
        ///  VT_I2               2 byte signed int
        ///  VT_I4               4 byte signed int
        ///  VT_UI1              unsigned char
        ///  VT_UI2              unsigned short
        ///  VT_UI4              unsigned long
        ///  VT_INT              signed machine int
        ///  VT_UINT             unsigned machine int
        ///  VT_R4               4 byte real
        ///  VT_R8               8 byte real
        ///  VT_BSTR             OLE Automation string
        ///  VT_BOOL             True=-1, False=0
        ///  VT_DATE             date
        ///  VT_UNKNOWN          IUnknown
        ///
        ///  Not yet implemented coversion methods for the below types:
        ///  VT_CY               currency
        ///  VT_DISPATCH         IDispatch
        ///  VT_ERROR            SCODE
        ///  VT_VARIANT          VARIANT
        ///  VT_DECIMAL          16 byte fixed point
        ///  VT_RECORD           user defined type
        ///  VT_ARRAY            SAFEARRAY*
        ///  VT_BYREF            void* for local use
        ///
        ////////////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// Assigns a value into the variant based off the explict template
        ///   VARTYPE passed through the template type
        ///
        /// For each possible VARTYPE, the VarTypeConverter struct defines
        ///   the allowable input type (matching the VARTYPE)
        ///
        /// On failure, can throw an std::exception (std::bad_alloc) or
        ///   ntl::Exception for types that need conversion (non-integers).
        ///
        ////////////////////////////////////////////////////////////////////////////////
        template <typename T>
        ComVariant& assign(_In_ ntl::ComPtr<T> _t)
        {
            ComVariant temp;
            temp.assign_impl(_t);
            this->swap(temp);
            return *this;
        }
        template <typename T>
        ComVariant& assign(_In_ std::vector<ntl::ComPtr<T>> _t)
        {
            ComVariant temp;
            temp.assign_impl(_t);
            this->swap(temp);
            return *this;
        }
        template <VARTYPE VT>
        ComVariant& assign(_In_ typename VarTypeConverter<VT>::assign_type _t)
        {
            ComVariant temp;
            temp.assign_impl(_t);
            this->swap(temp);
            return *this;
        }


        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// T retrieve<T>()
        ///
        /// inlined accessor to the internal value of the VARIANT
        /// - offers simpler usage model than the below [out] ptr
        ///
        ////////////////////////////////////////////////////////////////////////////////
        template <typename T>
        T retrieve()
        {
            T t;
            return retrieve(&t);
        }
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// T& retrieve(T*)
        ///
        /// Allows retrieval of data from within the variant
        /// Note that returns the [out] value directly - allowing inlining the call.
        /// e.g.
        ///
        /// int data;
        /// printf("Variant integer - %d", retrieve(&data));
        ///
        /// If types are incompatible, will throw a ntl::Exception
        ///
        ////////////////////////////////////////////////////////////////////////////////
        signed char& retrieve(_Out_ signed char * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = static_cast<signed char>(variant.boolVal);
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for char", L"ComVariantl::retrieve(signed char)", false);
            }
            return *_data;
        }
        unsigned char& retrieve(_Out_ unsigned char * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = static_cast<unsigned char>(variant.boolVal);
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for unsigned char", L"ComVariantl::retrieve(unsigned char)", false);
            }
            return *_data;
        }
        signed short& retrieve(_Out_ signed short * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = variant.boolVal;
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            case VT_I2:
                *_data = variant.iVal;
                break;
            case VT_UI2:
                *_data = variant.uiVal;
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for short", L"ComVariantl::retrieve(signed short)", false);
            }
            return *_data;
        }
        unsigned short& retrieve(_Out_ unsigned short * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = variant.boolVal;
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            case VT_I2:
                *_data = variant.iVal;
                break;
            case VT_UI2:
                *_data = variant.uiVal;
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for unsigned short", L"ComVariantl::retrieve(unsigned short)", false);
            }
            return *_data;
        }
        signed long& retrieve(_Out_ signed long * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = variant.boolVal;
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            case VT_I2:
                *_data = variant.iVal;
                break;
            case VT_UI2:
                *_data = variant.uiVal;
                break;
            case VT_I4:
                *_data = variant.lVal;
                break;
            case VT_UI4:
                *_data = variant.ulVal;
                break;
            case VT_INT:
                *_data = variant.intVal;
                break;
            case VT_UINT:
                *_data = variant.uintVal;
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for long", L"ComVariant::retrieve(signed long)", false);
            }
            return *_data;
        }
        unsigned long& retrieve(_Out_ unsigned long * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = variant.boolVal;
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            case VT_I2:
                *_data = variant.iVal;
                break;
            case VT_UI2:
                *_data = variant.uiVal;
                break;
            case VT_I4:
                *_data = variant.lVal;
                break;
            case VT_UI4:
                *_data = variant.ulVal;
                break;
            case VT_INT:
                *_data = variant.intVal;
                break;
            case VT_UINT:
                *_data = variant.uintVal;
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for unsigned long", L"ComVariant::retrieve(unsigned long)", false);
            }
            return *_data;
        }
        signed int& retrieve(_Out_ signed int * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = variant.boolVal;
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            case VT_I2:
                *_data = variant.iVal;
                break;
            case VT_UI2:
                *_data = variant.uiVal;
                break;
            case VT_I4:
                *_data = variant.lVal;
                break;
            case VT_UI4:
                *_data = variant.ulVal;
                break;
            case VT_INT:
                *_data = variant.intVal;
                break;
            case VT_UINT:
                *_data = variant.uintVal;
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for int", L"ComVariant::retrieve(signed int)", false);
            }
            return *_data;
        }
        unsigned int& retrieve(_Out_ unsigned int * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = variant.boolVal;
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            case VT_I2:
                *_data = variant.iVal;
                break;
            case VT_UI2:
                *_data = variant.uiVal;
                break;
            case VT_I4:
                *_data = variant.lVal;
                break;
            case VT_UI4:
                *_data = variant.ulVal;
                break;
            case VT_INT:
                *_data = variant.intVal;
                break;
            case VT_UINT:
                *_data = variant.uintVal;
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for unsigned int", L"ComVariant::retrieve(unsigned int)", false);
            }
            return *_data;
        }
        signed long long& retrieve(_Out_ signed long long * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = variant.boolVal;
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            case VT_I2:
                *_data = variant.iVal;
                break;
            case VT_UI2:
                *_data = variant.uiVal;
                break;
            case VT_I4:
                *_data = variant.lVal;
                break;
            case VT_UI4:
                *_data = variant.ulVal;
                break;
            case VT_INT:
                *_data = variant.intVal;
                break;
            case VT_UINT:
                *_data = variant.uintVal;
                break;
            case VT_I8:
                *_data = variant.llVal;
                break;
            case VT_UI8:
                *_data = variant.ullVal;
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for long", L"ComVariant::retrieve(signed long)", false);
            }
            return *_data;
        }
        unsigned long long& retrieve(_Out_ unsigned long long * _data) const
        {
            // allow any convertable integer types as long as not losing data
            switch (variant.vt) {
            case VT_BOOL:
                *_data = variant.boolVal;
                break;
            case VT_I1:
                *_data = variant.cVal;
                break;
            case VT_UI1:
                *_data = variant.bVal; // BYTE == unsigned char
                break;
            case VT_I2:
                *_data = variant.iVal;
                break;
            case VT_UI2:
                *_data = variant.uiVal;
                break;
            case VT_I4:
                *_data = variant.lVal;
                break;
            case VT_UI4:
                *_data = variant.ulVal;
                break;
            case VT_INT:
                *_data = variant.intVal;
                break;
            case VT_UINT:
                *_data = variant.uintVal;
                break;
            case VT_I8:
                *_data = variant.llVal;
                break;
            case VT_UI8:
                *_data = variant.ullVal;
                break;
            default:
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for unsigned long", L"ComVariant::retrieve(unsigned long)", false);
            }
            return *_data;
        }
        float& retrieve(_Out_ float * _data) const
        {
            if (variant.vt != VT_R4) {
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for float", L"ComVariant::retrieve(float)", false);
            }
            *_data = variant.fltVal;
            return *_data;
        }
        double& retrieve(_Out_ double * _data) const
        {
            if ((variant.vt != VT_R4) && (variant.vt != VT_R8)) {
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for double", L"ComVariant::retrieve(double)", false);
            }
            *_data = variant.dblVal;
            return *_data;
        }
        bool& retrieve(_Out_ bool * _data) const
        {
            if (variant.vt != VT_BOOL) {
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for bool", L"ComVariant::retrieve(bool)", false);
            }
            *_data = (variant.boolVal) ? true : false;
            return *_data;
        }
        ntl::ComBstr& retrieve(_Inout_ ntl::ComBstr * _data) const
        {
            if (variant.vt != VT_BSTR) {
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for ntl::ComBstr", L"ComVariant::retrieve(ntl::ComBstr)", false);
            }
            _data->set(variant.bstrVal);
            return *_data;
        }
        std::wstring& retrieve(_Inout_ std::wstring * _data) const
        {
            if (variant.vt != VT_BSTR) {
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for std::wstring", L"ComVariant::retrieve(std::wstring)", false);
            }
            if (variant.bstrVal != nullptr) {
                _data->assign(variant.bstrVal);
            } else {
                _data->clear();
            }
            return *_data;
        }
        SYSTEMTIME& retrieve(_Out_ SYSTEMTIME * _data) const
        {
            if (variant.vt != VT_DATE) {
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for SYSTEMTIME", L"ComVariant::retrieve(SYSTEMTIME)", false);
            }
            if (!::VariantTimeToSystemTime(variant.date, _data)) {
                throw ntl::Exception(::GetLastError(), L"SystemTimeToVariantTime", L"ComVariant::retrieve(SYSTEMTIME)", false);
            }
            return *_data;
        }
        FILETIME& retrieve(_Out_ FILETIME* _data) const
        {
            if (variant.vt != VT_DATE) {
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for FILETIME", L"ComVariant::retrieve(FILETIME)", false);
            }
            SYSTEMTIME st;
            if (!::VariantTimeToSystemTime(variant.date, &st)) {
                throw ntl::Exception(::GetLastError(), L"SystemTimeToVariantTime", L"ComVariant::retrieve(FILETIME)", false);
            }
            if (!::SystemTimeToFileTime(&st, _data)) {
                throw ntl::Exception(::GetLastError(), L"SystemTimeToFileTime", L"ComVariant::retrieve(FILETIME)", false);
            }
            return *_data;
        }
        VARIANT& retrieve(_Inout_ VARIANT* _data) const
        {
            // don't modify the member variant until known success
            ComVariant temp(&variant);
            // move the copy of this->variant into _data,
            // - and move _data into temp, where it will be cleared on exit
            using std::swap;
            swap(temp.variant, *_data);
            return *_data;
        }
        ComVariant& retrieve(_Inout_ ComVariant* _data) const
        {
            _data->set(&variant);
            return *_data;
        }
        std::vector<std::wstring>& retrieve(_Inout_ std::vector<std::wstring>* _data) const
        {
            if (variant.vt != (VT_BSTR | VT_ARRAY)) {
                throw ntl::Exception(
                    variant.vt,
                    L"Mismatching VARTYPE for std::vector<std::wstring>",
                    L"ComVariantl::retrieve(std::vector<std::wstring>)",
                    false);
            }

            BSTR *stringArray;
            HRESULT hr = ::SafeArrayAccessData(variant.parray, reinterpret_cast<void **>(&stringArray));
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"SafeArrayAccessData", L"ComVariant::retrieve(std::vector<std::wstring>)", false);
            }

            // scope guard will guarantee SafeArrayUnaccessData is called on variant.parray even in the face of an exception
            ScopeGuard(unaccessArray, {::SafeArrayUnaccessData(variant.parray); });

            // don't modify the out param should an exception be thrown - don't leave the user with bogus data
            std::vector<std::wstring> tempData;
            for (unsigned loop = 0; loop < variant.parray->rgsabound[0].cElements; ++loop) {
                tempData.push_back(stringArray[loop]);
            }

            // swap the safely constructed data to the out param - a no-fail operation
            _data->swap(tempData);
            return *_data;
        }

        std::vector<unsigned long>& retrieve(_Out_ std::vector<unsigned long> * _data) const
        {
            if (variant.vt != (VT_UI4 | VT_ARRAY)) {
                throw ntl::Exception(
                    variant.vt,
                    L"Mismatching VARTYPE for std::vector<unsigned long>",
                    L"ntl::ComVariant::retrieve(std::vector<unsigned long>)",
                    false);
            }

            unsigned long *intArray;
            HRESULT hr = ::SafeArrayAccessData(variant.parray, reinterpret_cast<void **>(&intArray));
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"SafeArrayAccessData", L"ntl::ComVariant::retrieve(std::vector<std::wstring>)", false);
            }

            // scope guard will guarantee SafeArrayUnaccessData is called on variant.parray even in the face of an exception
            ScopeGuard(unaccessArray, {::SafeArrayUnaccessData(variant.parray); });

            // don't modify the out param should an exception be thrown - don't leave the user with bogus data
            std::vector<unsigned long> tempData;
            for (unsigned loop = 0; loop < variant.parray->rgsabound[0].cElements; ++loop) {
                tempData.push_back(intArray[loop]);
            }

            // swap the safely constructed data to the out param - a no-fail operation
            _data->swap(tempData);
            return *_data;
        }

        template <typename T>
        ntl::ComPtr<T>& retrieve(_Inout_ ntl::ComPtr<T>* _data) const
        {
            if (variant.vt != VT_UNKNOWN) {
                throw ntl::Exception(variant.vt, L"Mismatching VARTYPE for ntl::ComPtr<T>", L"ComVariant::retrieve(ntl::ComPtr<T>)", false);
            }

            HRESULT hr = variant.punkVal->QueryInterface(__uuidof(T), reinterpret_cast<void**>(_data->get_addr_of()));
            if (FAILED(hr)) {
                throw ntl::Exception(variant.vt, L"IUnknown::QueryInterface", L"ComVariant::retrieve(ntl::ComPtr<T>)", false);
            }

            return *_data;
        }

        template <typename T>
        std::vector<ntl::ComPtr<T>>& retrieve(_Inout_ std::vector<ntl::ComPtr<T>>* _data) const
        {
            if (variant.vt != (VT_UNKNOWN | VT_ARRAY)) {
                throw ntl::Exception(
                    variant.vt,
                    L"Mismatching VARTYPE for std::vector<ntl::ComPtr<T>>",
                    L"ComVariant::retrieve(std::vector<ntl::ComPtr<T>>)",
                    false);
            }

            IUnknown** iUnknownArray;
            HRESULT hr = ::SafeArrayAccessData(variant.parray, reinterpret_cast<void **>(&iUnknownArray));
            if (FAILED(hr)) {
                throw ntl::Exception(hr, L"SafeArrayAccessData", L"ntl::ComVariant::retrieve(std::vector<ntl::ComPtr<T>>)", false);
            }

            // scope guard will guarantee SafeArrayUnaccessData is called on variant.parray even in the face of an exception
            ScopeGuard(unaccessArray, {::SafeArrayUnaccessData(variant.parray); });

            // don't modify the out param should an exception be thrown - don't leave the user with bogus data
            std::vector<ntl::ComPtr<T>> tempData;
            for (unsigned loop = 0; loop < variant.parray->rgsabound[0].cElements; ++loop) {
                ntl::ComPtr<T> tempPtr;

                hr = iUnknownArray[loop]->QueryInterface(__uuidof(T), reinterpret_cast<void**>(tempPtr.get_addr_of()));
                if (FAILED(hr)) {
                    throw ntl::Exception(hr, L"IUnknown::QueryInterface", L"ComVariant::retrieve(std::vector<ntl::ComPtr<T>>)", false);
                }

                tempData.push_back(tempPtr);
            }

            // swap the safely constructed data to the out param - a no-fail operation
            _data->swap(tempData);
            return *_data;
        }

    private:
        VARIANT variant;

        void assign_impl(bool _value) NOEXCEPT
        {
            variant.boolVal = (_value) ? VARIANT_TRUE : VARIANT_FALSE;
            variant.vt = VT_BOOL;
        }
        void assign_impl(signed char _value) NOEXCEPT
        {
            variant.cVal = _value;
            variant.vt = VT_I1;
        }
        void assign_impl(unsigned char _value) NOEXCEPT
        {
            variant.bVal = _value; // BYTE == unsigned char
            variant.vt = VT_UI1;
        }
        void assign_impl(signed short _value) NOEXCEPT
        {
            variant.iVal = _value;
            variant.vt = VT_I2;
        }
        void assign_impl(unsigned short _value) NOEXCEPT
        {
            variant.uiVal = _value;
            variant.vt = VT_UI2;
        }
        void assign_impl(signed long _value) NOEXCEPT
        {
            variant.lVal = _value;
            variant.vt = VT_I4;
        }
        void assign_impl(unsigned long _value) NOEXCEPT
        {
            variant.ulVal = _value;
            variant.vt = VT_UI4;
        }
        void assign_impl(signed int _value) NOEXCEPT
        {
            variant.intVal = _value;
            variant.vt = VT_INT;
        }
        void assign_impl(unsigned int _value) NOEXCEPT
        {
            variant.uintVal = _value;
            variant.vt = VT_UINT;
        }
        void assign_impl(signed long long _value) NOEXCEPT
        {
            variant.llVal = _value;
            variant.vt = VT_I8;
        }
        void assign_impl(unsigned long long _value) NOEXCEPT
        {
            variant.ullVal = _value;
            variant.vt = VT_UI8;
        }
        void assign_impl(float _value) NOEXCEPT
        {
            variant.fltVal = _value;
            variant.vt = VT_R4;
        }
        void assign_impl(double _value) NOEXCEPT
        {
            variant.dblVal = _value;
            variant.vt = VT_R8;
        }

        void assign_impl(_In_opt_ LPCWSTR _data)
        {
            BSTR temp = nullptr;
            if (_data != nullptr) {
                temp = ::SysAllocString(_data);
                if (nullptr == temp) {
                    throw std::bad_alloc();
                }
            }
            variant.bstrVal = temp;
            variant.vt = VT_BSTR;
        }

        void assign_impl(SYSTEMTIME _data)
        {
            DOUBLE time;
            if (!::SystemTimeToVariantTime(&_data, &time)) {
                throw ntl::Exception(::GetLastError(), L"SystemTimeToVariantTime", L"ComVariant::assign", false);
            }
            variant.date = time;
            variant.vt = VT_DATE;
        }

        void assign_impl(const std::vector<std::wstring>& _data)
        {
            SAFEARRAY* temp_safe_array = ::SafeArrayCreateVector(VT_BSTR, 0, static_cast<ULONG>(_data.size()));
            if (nullptr == temp_safe_array) {
                throw std::bad_alloc();
            }
            // store the SAFEARRY in an exception safe container
            ScopeGuard(guard_array, {::SafeArrayDestroy(temp_safe_array); });

            for (size_t loop = 0; loop < _data.size(); ++loop) {
                //
                // SafeArrayPutElement requires an array of indexes for each dimension of the array
                // - in this case, we have a 1-dimensional array, thus an array of 1 LONG - assigned to the loop variable
                //
                long index[1] = {static_cast<long>(loop)};

                ntl::ComVariant stringVariant;
                stringVariant.assign<VT_BSTR>(_data[loop].c_str());
                HRESULT hr = ::SafeArrayPutElement(temp_safe_array, index, stringVariant->bstrVal);
                if (FAILED(hr)) {
                    throw ntl::Exception(hr, L"SafeArrayPutElement", L"ComVariant::assign(std::vector<std::wstring>)", false);
                }
            }
            // don't free the SAFEARRAY on success - its lifetime is transferred to this->variant
            guard_array.dismiss();
            variant.parray = temp_safe_array;
            variant.vt = VT_BSTR | VT_ARRAY;
        }

        void assign_impl(const std::vector<unsigned long>& _data)
        {
            SAFEARRAY* temp_safe_array = ::SafeArrayCreateVector(VT_UI4, 0, static_cast<ULONG>(_data.size()));
            if (nullptr == temp_safe_array) {
                throw std::bad_alloc();
            }
            // store the SAFEARRY in an exception safe container
            ScopeGuard(guard_array, {::SafeArrayDestroy(temp_safe_array); });

            for (size_t loop = 0; loop < _data.size(); ++loop) {
                //
                // SafeArrayPutElement requires an array of indexes for each dimension of the array
                // - in this case, we have a 1-dimensional array, thus an array of 1 LONG - assigned to the loop variable
                //
                long index[1] = {static_cast<long>(loop)};

                HRESULT hr = ::SafeArrayPutElement(temp_safe_array, index, const_cast<unsigned long *>(&(_data[loop])));
                if (FAILED(hr)) {
                    throw ntl::Exception(hr, L"SafeArrayPutElement", L"ntl::ComVariant::assign(std::vector<unsigned long>)", false);
                }
            }
            // don't free the SAFEARRAY on success - its lifetime is transferred to this->variant
            guard_array.dismiss();
            variant.parray = temp_safe_array;
            variant.vt = VT_UI4 | VT_ARRAY;
        }

        void assign_impl(const std::vector<unsigned short>& _data)
        {
            //WMI marshaler complaines type mismatch using VT_UI2 | VT_ARRAY, and VT_I4 | VT_ARRAY works fine.
            SAFEARRAY* temp_safe_array = ::SafeArrayCreateVector(VT_I4, 0, static_cast<ULONG>(_data.size()));
            if (nullptr == temp_safe_array) {
                throw std::bad_alloc();
            }
            // store the SAFEARRY in an exception safe container
            ScopeGuard(guard_array, {::SafeArrayDestroy(temp_safe_array); });

            for (size_t loop = 0; loop < _data.size(); ++loop) {
                //
                // SafeArrayPutElement requires an array of indexes for each dimension of the array
                // - in this case, we have a 1-dimensional array, thus an array of 1 LONG - assigned to the loop variable
                //
                long index[1] = {static_cast<long>(loop)};
                // Expand unsigned short to long because SafeArrayPutElement takes the memory with size equals to the element type
                long value = _data[loop];
                HRESULT hr = ::SafeArrayPutElement(temp_safe_array, index, &value);
                if (FAILED(hr)) {
                    throw ntl::Exception(hr, L"SafeArrayPutElement", L"ntl::ComVariant::assign(std::vector<unsigned short>)", false);
                }
            }
            // don't free the SAFEARRAY on success - its lifetime is transferred to this->variant
            guard_array.dismiss();
            variant.parray = temp_safe_array;
            variant.vt = VT_I4 | VT_ARRAY;
        }

        void assign_impl(const std::vector<unsigned char>& _data)
        {
            SAFEARRAY* temp_safe_array = ::SafeArrayCreateVector(VT_UI1, 0, static_cast<ULONG>(_data.size()));
            if (nullptr == temp_safe_array) {
                throw std::bad_alloc();
            }
            // store the SAFEARRY in an exception safe container
            ScopeGuard(guard_array, {::SafeArrayDestroy(temp_safe_array); });

            for (size_t loop = 0; loop < _data.size(); ++loop) {
                //
                // SafeArrayPutElement requires an array of indexes for each dimension of the array
                // - in this case, we have a 1-dimensional array, thus an array of 1 LONG - assigned to the loop variable
                //
                long index[1] = {static_cast<long>(loop)};

                HRESULT hr = ::SafeArrayPutElement(temp_safe_array, index, const_cast<unsigned char *>(&(_data[loop])));
                if (FAILED(hr)) {
                    throw ntl::Exception(hr, L"SafeArrayPutElement", L"ntl::ComVariant::assign(std::vector<unsigned char>)", false);
                }
            }
            // don't free the SAFEARRAY on success - its lifetime is transferred to this->variant
            guard_array.dismiss();
            variant.parray = temp_safe_array;
            variant.vt = VT_UI1 | VT_ARRAY;
        }

        template <typename T>
        void assign_impl(ntl::ComPtr<T>& _value) NOEXCEPT
        {
            variant.punkVal = _value.get_IUnknown();
            variant.punkVal->AddRef();
            variant.vt = VT_UNKNOWN;
        }

        template <typename T>
        void assign_impl(std::vector<ntl::ComPtr<T>>& _data)
        {
            SAFEARRAY* temp_safe_array = ::SafeArrayCreateVector(VT_UNKNOWN, 0, static_cast<ULONG>(_data.size()));
            if (nullptr == temp_safe_array) {
                throw std::bad_alloc();
            }
            // store the SAFEARRY in an exception safe container
            ScopeGuard(guard_array, {::SafeArrayDestroy(temp_safe_array); });

            // to be exception safe, AddRef every object before trying to add them to the safe-array
            // - on exception, the ScopeGuard will Release these extra references
            for (auto& ptr : _data) {
                ptr->AddRef();
            }
            ScopeGuard(guard_ntl::ComPtr, {
                for (auto& ptr : _data) {
                    ptr->Release();
                }
            });

            for (size_t loop = 0; loop < _data.size(); ++loop) {
                //
                // SafeArrayPutElement requires an array of indexes for each dimension of the array
                // - in this case, we have a 1-dimensional array, thus an array of 1 LONG - assigned to the loop variable
                //
                long index[1] = {static_cast<long>(loop)};

                HRESULT hr = ::SafeArrayPutElement(temp_safe_array, index, _data[loop].get_IUnknown());
                if (FAILED(hr)) {
                    throw ntl::Exception(hr, L"SafeArrayPutElement", L"ntl::ComVariant::assign(std::vector<ntl::ComPtr<T>>)", false);
                }
            }

            // don't free the SAFEARRAY or Release the IUnknown* on success 
            // - the lifetime of both are now transferred to this->variant
            guard_ntl::ComPtr.dismiss();
            guard_array.dismiss();

            variant.parray = temp_safe_array;
            variant.vt = VT_UNKNOWN | VT_ARRAY;
        }
    };

    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// non-member swap()
    ///
    /// Required so the correct swap() function is called if the caller writes:
    ///  (which can be done in STL algorithms as well).
    ///
    /// swap(var1, var2);
    ///
    ////////////////////////////////////////////////////////////////////////////////
    inline void swap(_Inout_ ntl::ComVariant& _lhs, _Inout_ ntl::ComVariant& _rhs) NOEXCEPT
    {
        _lhs.swap(_rhs);
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///
    /// String helper implementations
    ///
    /// Defining these here allows String functions such as ordinal_equals to
    /// work with ComBstrs as arguments without requiring every user of String
    /// to pull in this header
    ///
    ////////////////////////////////////////////////////////////////////////////////
    namespace String
    {
        namespace _detail
        {
            inline _Ret_z_ const wchar_t* convert_to_ptr(const ntl::ComBstr& source)
            {
                return source.c_str();
            }

            inline size_t get_string_length(const ntl::ComBstr& source)
            {
                return source.size();
            }
        }
    } // namespace String::_detail

} // namespace ntl
