// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <stdlib.h>
#include <wchar.h>
#include <intrin.h>
#include <string>
#include <exception>
// os headers
#include <Windows.h>
// ntl headers
#include "ntlVersionConversion.hpp"

namespace ntl {

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //  Exception
    //
    //  Derived from the base class of exception.
    //
    //  A class to provide deeper exception details including:
    //    - a wide-char version of exception::what()
    //    - an optional unsigned long variable to store a numeric error code
    //  - an optional wide-char string for the location of the exception throwing
    //  - a method to translate Win32 error codes to a wide-char string
    //
    //  All methods are specified NOEXCEPT - no method can fail
    //  - should alloc failures occur, an empty string is returned the the caller
    //
    //  ** Note on methods returning pointers to const buffers:
    //  Just as with the wstring::c_str() and string::c_str() methods, as well as the
    //    virtual method exception::what() that class this overrides:
    //    The semantics of the pointers returned:
    //      - must remain const (modifying them can and will corrupt the internals of the object)
    //      - only survive the lifetime of the object
    //  Where the semantics differ from the wstring::c_str() and string::c_str() methods
    //    is that once the const pointer is created, it is guaranteed not to change 
    //    for the lifetime of the Exception object.
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    class Exception : public std::exception {
    public:
        // constructors
        explicit Exception(const unsigned long _ulCode) NOEXCEPT;
        explicit Exception(_In_ LPCSTR _wszMessage, const bool _bMessageCopy = true) NOEXCEPT;
        explicit Exception(_In_ LPCWSTR _wszMessage, const bool _bMessageCopy = true) NOEXCEPT;
        explicit Exception(const std::wstring& _wsMessage) NOEXCEPT;
        explicit Exception(const std::string& _sMessage) NOEXCEPT;

        explicit Exception(const unsigned long _ulCode, _In_ LPCWSTR _wszMessage, const bool _bMessageCopy = true) NOEXCEPT;
        explicit Exception(const unsigned long _ulCode, _In_ LPCWSTR _wszMessage, _In_ LPCWSTR _wszLocation, const bool _bBothStringCopy = true) NOEXCEPT;
        explicit Exception(const unsigned long _ulCode, _In_ LPCSTR _szMessage, const bool _bMessageCopy = true) NOEXCEPT;
        explicit Exception(const unsigned long _ulCode, const std::wstring& _wsMessage) NOEXCEPT;
        explicit Exception(const unsigned long _ulCode, const std::string& _sMessage) NOEXCEPT;

        Exception() NOEXCEPT;
        explicit Exception(const std::exception& e) NOEXCEPT;
        Exception(const Exception& e) NOEXCEPT;

        // operator= implementations

        // ReSharper disable once CppHidingFunction
        virtual Exception& operator=(const std::exception& e) NOEXCEPT;
        virtual Exception& operator=(const Exception& e) NOEXCEPT;

        // destructor
        virtual ~Exception() NOEXCEPT;

        // public accessors
        virtual const unsigned long why() const NOEXCEPT;
        virtual const char* what() const NOEXCEPT override;
        virtual const wchar_t* what_w() const NOEXCEPT;
        virtual const wchar_t* where_w() const NOEXCEPT;

        virtual const wchar_t* translation_w() const NOEXCEPT;

        // no-fail operators
        virtual void reset() NOEXCEPT;

    private:
        // private functions to alloc and copy string buffers
        char* calloc_and_copy_s(_In_ LPCSTR tSrc) const NOEXCEPT;
        wchar_t* calloc_and_copy_w(_In_ LPCWSTR tSrc) const NOEXCEPT;

        // not making swap() public since the base class does not implement swap()
        // - and that risks getting our objects out of sync
        // - using this internally in a controlled manner with temporaries
        void swap(Exception& e) NOEXCEPT;

        // private multi-byte/wide-char translators
        void wszMessageToszMessage() NOEXCEPT;
        void szMessageTowszMessage() NOEXCEPT;

        // private member variables
        const char* szMessage = nullptr;
        const wchar_t* wszMessage = nullptr;
        const wchar_t* wszLocation = nullptr;
        mutable wchar_t* wszTranslation = nullptr;
        unsigned long ulCode = 0;
        bool bMessageCopy_s = false;
        bool bMessageCopy_w = false;
        bool bLocationCopy_w = false;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // Constructors
    //
    // - all provide a best attempt at preserving the original error strings
    // - allows char* strings for the "message" to maintain compatibility with exception::what()
    // - allows for an optional bool value to be passed to specify whether to make a copy of the string
    //   - if no copy is made, the pointer won't be deleted on destruction
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline Exception::Exception() NOEXCEPT : 
        std::exception("") // initialize the base c'tor
    {
    }

    inline Exception::Exception(unsigned long _ulCode) NOEXCEPT : 
        std::exception(""), // initialize the base c'tor
        ulCode(_ulCode) // initializing with args
    {
    }

    inline Exception::Exception(_In_ LPCSTR _szMessage, const bool _bMessageCopy) NOEXCEPT:
        std::exception(""), // initialize the base c'tor
        szMessage(_szMessage), // initializing with args
        bMessageCopy_s(_bMessageCopy)  // initializing with args
    {
        if (bMessageCopy_s) {
            szMessage = calloc_and_copy_s(_szMessage);
        }
        // build the corresponding wchar_t string
        szMessageTowszMessage();
    }

    inline Exception::Exception(_In_ LPCWSTR _wszMessage, const bool _bMessageCopy) NOEXCEPT :
        std::exception(""), // initialize the base c'tor
        wszMessage(_wszMessage), // initializing with args
        bMessageCopy_w(_bMessageCopy) // initializing with args
    {
        if (bMessageCopy_w) {
            wszMessage = calloc_and_copy_w(_wszMessage);
        }
        // build the corresponding char string
        wszMessageToszMessage();
    }

    inline Exception::Exception(const std::wstring& _wsMessage) NOEXCEPT : 
        std::exception("") // initialize the base c'tor
    {
        wszMessage = calloc_and_copy_w(_wsMessage.c_str());
        bMessageCopy_w = true;
        // build the corresponding char string
        wszMessageToszMessage();
    }

    inline Exception::Exception(const std::string& _sMessage) NOEXCEPT : 
        std::exception("") // initialize the base c'tor
    {
        szMessage = calloc_and_copy_s(_sMessage.c_str());
        bMessageCopy_s = true;
        // build the corresponding wchar_t string
        szMessageTowszMessage();
    }

    inline Exception::Exception(const unsigned long _ulCode, _In_ LPCWSTR _wszMessage, const bool _bMessageCopy) NOEXCEPT :
        std::exception(""), // initialize the base c'tor
        ulCode(_ulCode), // initializing with args
        wszMessage(_wszMessage), // initializing with args
        bMessageCopy_w(_bMessageCopy) // initializing with args
    {
        if (bMessageCopy_w) {
            wszMessage = calloc_and_copy_w(_wszMessage);
        }
        // build the corresponding char string
        wszMessageToszMessage();
    }

    inline Exception::Exception(const unsigned long _ulCode, _In_ LPCWSTR _wszMessage, _In_ LPCWSTR _wszLocation, const bool _bBothStringCopy) NOEXCEPT :
        std::exception(""), // initialize the base c'tor
        ulCode(_ulCode),
        wszMessage(_wszMessage), // initializing with args
        wszLocation(_wszLocation), // initializing with args
        bMessageCopy_w(_bBothStringCopy), // initializing with args
        bLocationCopy_w(_bBothStringCopy) // initializing with args
    {
        if (_bBothStringCopy) {
            wszMessage = calloc_and_copy_w(_wszMessage);
            wszLocation = calloc_and_copy_w(_wszLocation);
        }
        // build the corresponding char string
        wszMessageToszMessage();
    }

    inline Exception::Exception(const unsigned long _ulCode, _In_ LPCSTR _szMessage, const bool _bMessageCopy) NOEXCEPT :
        std::exception(_szMessage), // initialize the base c'tor
        ulCode(_ulCode), // initializing with args
        szMessage(_szMessage), // initializing with args
        bMessageCopy_s(_bMessageCopy) // initializing with args
    {
        if (bMessageCopy_s) {
            szMessage = calloc_and_copy_s(_szMessage);
        }
        // build the corresponding wchar_t string
        szMessageTowszMessage();
    }

    inline Exception::Exception(const unsigned long _ulCode, const std::wstring& _wsMessage) NOEXCEPT :
        std::exception(""), // initialize the base c'tor
        ulCode(_ulCode) // initializing with args
    {
        wszMessage = calloc_and_copy_w(_wsMessage.c_str());
        bMessageCopy_w = true;
        // build the corresponding char string
        wszMessageToszMessage();
    }

    inline Exception::Exception(const unsigned long _ulCode, const std::string& _sMessage) NOEXCEPT :
        std::exception(""), // initialize the base c'tor
        ulCode(_ulCode) // initializing with args
    {
        szMessage = calloc_and_copy_s(_sMessage.c_str());
        bMessageCopy_s = true;
        // build the corresponding wchar_t string
        szMessageTowszMessage();
    }

    inline Exception::Exception(const std::exception& e) NOEXCEPT : 
        std::exception(e) // initialize the base c'tor
    {
        szMessage = calloc_and_copy_s(e.what());
        bMessageCopy_s = true;
        // build the corresponding wchar_t string
        szMessageTowszMessage();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // Copy constructor
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline Exception::Exception(const Exception& e) NOEXCEPT : 
        std::exception(""), // initialize the base c'tor
        ulCode(e.ulCode),
        szMessage(e.szMessage),
        wszMessage(e.wszMessage),
        wszLocation(e.wszLocation),
        wszTranslation(nullptr), // don't copy this buffer allocated by FormatString
        bMessageCopy_s(e.bMessageCopy_s),
        bMessageCopy_w(e.bMessageCopy_w),
        bLocationCopy_w(e.bLocationCopy_w)
    {
        if (bMessageCopy_s) {
            szMessage = calloc_and_copy_s(e.szMessage);
        }

        if (bMessageCopy_w) {
            wszMessage = calloc_and_copy_w(e.wszMessage);
        }

        if (bLocationCopy_w) {
            wszLocation = calloc_and_copy_w(e.wszLocation);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // operator= assignment
    // - safe even if (this == &e)
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline Exception& Exception::operator=(const std::exception& e) NOEXCEPT
    {
        //
        // Make a copy before setting local variables
        //
        Exception temp(e);
        //
        // swap and return
        //
        this->swap(temp);
        return *this;
    }
    inline Exception& Exception::operator=(const Exception& e) NOEXCEPT
    {
        //
        // Make a copy before setting local variables
        //
        Exception temp(e);
        //
        // swap and return
        //
        this->swap(temp);
        return *this;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // Destructor
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline Exception::~Exception() NOEXCEPT
    {
        this->Exception::reset();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // reset()
    // - no-fail operation to clear all members
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline void Exception::reset() NOEXCEPT
    {
        if (this->bMessageCopy_s)  ::free(const_cast<void*>(static_cast<const void*>(this->szMessage)));
        if (this->bMessageCopy_w)  ::free(const_cast<void*>(static_cast<const void*>(this->wszMessage)));
        if (this->bLocationCopy_w) ::free(const_cast<void*>(static_cast<const void*>(this->wszLocation)));
        ::free(static_cast<void*>(this->wszTranslation));

        this->ulCode = 0;
        this->szMessage = nullptr;
        this->wszMessage = nullptr;
        this->wszLocation = nullptr;
        this->wszTranslation = nullptr;
        this->bMessageCopy_s = false;
        this->bMessageCopy_w = false;
        this->bLocationCopy_w = false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // why()
    // - returns the unsigned long integer error code
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline const unsigned long Exception::why() const NOEXCEPT
    {
        return ulCode;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // what()
    // - returns the char* member for the reason for the failure
    // - virtual function called if called polymorphically from an std::exception object
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline const char * Exception::what() const NOEXCEPT
    {
        return (this->szMessage != nullptr) ? this->szMessage : "";
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // what_w()
    // - returns the wchar_t* member for the reason for the failure
    // - the "wide" version of std::exception::what()
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline const wchar_t* Exception::what_w() const NOEXCEPT
    {
        return (this->wszMessage != nullptr) ? this->wszMessage : L"";
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // where_w()
    // - returns the wchar_t* member for where the failure occured
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline const wchar_t* Exception::where_w() const NOEXCEPT
    {
        return (this->wszLocation != nullptr) ? this->wszLocation : L"";
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // translation_w()
    // - returns the wchar_t* member for the system translation of the error from this->why()
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline const wchar_t* Exception::translation_w() const NOEXCEPT
    {
        // If the translation has already been performed, return the existing value.
        if (nullptr != this->wszTranslation) {
            return this->wszTranslation;
        }

        static const DWORD cchBuffer = 1024;
        wchar_t* wszBuffer = static_cast<wchar_t*>(::calloc(cchBuffer, sizeof(wchar_t)));
        if (nullptr != wszBuffer) {
            // We carefully avoid using the FORMAT_MESSAGE_ALLOCATE_BUFFER flag.
            // It triggers a use of the LocalAlloc() function. LocalAlloc() and LocalFree() are in an API set that is obsolete.
            static const DWORD DWFLAGS =
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS |
                FORMAT_MESSAGE_MAX_WIDTH_MASK;

            DWORD dwReturn = ::FormatMessageW(
                DWFLAGS,
                nullptr, // just search the system
                this->ulCode,
                0, // allow for proper MUI language fallback
                wszBuffer,
                cchBuffer,
                nullptr
               );
            if (0 == dwReturn) {
                // Free the temporary buffer here, as it won't be assigned to wszTranslation to be freed later.
                ::free(static_cast<void*>(wszBuffer));
            } else {
                this->wszTranslation = wszBuffer;
            }
        }

        return (this->wszTranslation != nullptr) ? this->wszTranslation : L"";
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // wszMessageToszMessage()
    // - converts the wide-char message string to the multi-byte char string
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline void Exception::wszMessageToszMessage() NOEXCEPT
    {
        if (nullptr == this->wszMessage) {
            return;
        }

        int iResult = ::WideCharToMultiByte(
            CP_ACP,
            WC_NO_BEST_FIT_CHARS,
            this->wszMessage,
            -1,
            nullptr,
            0,
            nullptr,
            nullptr
           );
        if (iResult != 0) {
            char* temp_szMessage = static_cast<char*>(::calloc(iResult, sizeof(char)));
            if (temp_szMessage != nullptr) {
                iResult = ::WideCharToMultiByte(
                    CP_ACP,
                    WC_NO_BEST_FIT_CHARS,
                    this->wszMessage,
                    -1,
                    temp_szMessage,
                    iResult,
                    nullptr,
                    nullptr
                   );
                if (0 == iResult) {
                    // failed to populate the buffer, so should free it
                    free(temp_szMessage);
                } else {
                    this->szMessage = temp_szMessage;
                    this->bMessageCopy_s = true;
                }
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // szMessageTowszMessage()
    // - converts the multi-byte char message string to the wide-char string
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline void Exception::szMessageTowszMessage() NOEXCEPT
    {
        if (nullptr == this->szMessage) {
            return;
        }

        int iResult = ::MultiByteToWideChar(CP_ACP, 0, this->szMessage, -1, nullptr, 0);
        if (iResult != 0) {
            wchar_t* temp_wszMessage = static_cast<wchar_t*>(::calloc(iResult, sizeof(wchar_t)));
            if (temp_wszMessage != nullptr) {
                iResult = ::MultiByteToWideChar(CP_ACP, 0, this->szMessage, -1, temp_wszMessage, iResult);
                if (0 == iResult) {
                    // failed to populate the buffer, so should free it
                    free(temp_wszMessage);
                } else {
                    this->wszMessage = temp_wszMessage;
                    this->bMessageCopy_w = true;
                }
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // calloc_and_copy_s(char*)
    //
    // - allocates a buffer and copies the multi-byte char string
    // - returns the pointer to the allocated buffer
    //        - or nullptr if failed to allocate
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline char* Exception::calloc_and_copy_s(_In_ LPCSTR tSrc) const NOEXCEPT
    {
        // no need to copy a nullptr ptr
        if (nullptr == tSrc) {
            return nullptr;
        }

        size_t tSize = strlen(tSrc) + 1;
        char* tDest = static_cast<char*>(::calloc(tSize, sizeof(char)));
        if (tDest != nullptr) {
            errno_t err = strcpy_s(tDest, tSize, tSrc);
            if (err != 0) {
                free(tDest);
                tDest = nullptr;
            }
        }

        return tDest;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // calloc_and_copy_w(wchar_t*)
    //
    // - allocates a buffer and copies the wide-char string
    // - returns the pointer to the allocated buffer
    //        - or nullptr if failed to allocate
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline wchar_t* Exception::calloc_and_copy_w(_In_ LPCWSTR tSrc) const NOEXCEPT
    {
        // no need to copy a nullptr ptr
        if (nullptr == tSrc) {
            return nullptr;
        }

        size_t tSize = wcslen(tSrc) + 1;
        wchar_t* tDest = static_cast<wchar_t*>(::calloc(tSize, sizeof(wchar_t)));
        if (tDest != nullptr) {
            errno_t err = wcscpy_s(tDest, tSize, tSrc);
            if (err != 0) {
                free(tDest);
                tDest = nullptr;
            }
        }

        return tDest;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // swap()
    // - no-fail swap operation to swap 2 Exception objects
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    inline void Exception::swap(Exception& e) NOEXCEPT
    {
        using std::swap;
        swap(this->ulCode, e.ulCode);
        swap(this->szMessage, e.szMessage);
        swap(this->wszMessage, e.wszMessage);
        swap(this->wszLocation, e.wszLocation);
        swap(this->wszTranslation, e.wszTranslation);
        swap(this->bMessageCopy_s, e.bMessageCopy_s);
        swap(this->bMessageCopy_w, e.bMessageCopy_w);
        swap(this->bLocationCopy_w, e.bLocationCopy_w);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // FatalCondition
    //
    // Functions which serve as an "assert" to RaiseException should a condition be false.
    // Useful to when expressing invariants in ones code to make them more easily debuggable.
    // The relevant string details of the failed condition is written to the debugger before it breaks.
    // The string is additionally written to the STDERR file handle - if a console is waiting for it
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    // NTSTATUS being used: 1110 (E) for severity, c71f00d for facility & code (ntl)
    static const unsigned long FatalConditionExceptionCode = 0xec71f00d;
    namespace {
        ///
        /// the call to RaiseFailFastException is put into its own function
        ///  - so it can be described __analysis_noreturn for SAL
        ///
        __analysis_noreturn
        void FailFast(_In_ EXCEPTION_RECORD* _exr)
        {
            ::RaiseFailFastException(_exr, nullptr, 0);
        }
    }
    inline
        void __cdecl FatalConditionVa(const bool _condition, _Printf_format_string_ const wchar_t* _text, _In_ va_list _argptr) NOEXCEPT
    {
        if (_condition) {
            // write everyting out into a single string
            static const size_t exception_length = 512;
            wchar_t exception_text[exception_length] = { L'\0' };
            ::_vsnwprintf_s(exception_text, exception_length, _TRUNCATE, _text, _argptr);

            // first print it to the output stream
            ::fwprintf_s(stderr, exception_text);

            // then write it out to the debugger
            ::OutputDebugStringW(exception_text);

            // then raise the exception to break
            EXCEPTION_RECORD exr;
            ::ZeroMemory(&exr, static_cast<DWORD>(sizeof(exr)));
            exr.ExceptionCode = FatalConditionExceptionCode;
            exr.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
            exr.ExceptionRecord = nullptr;
            exr.ExceptionAddress = _ReturnAddress();
            exr.NumberParameters = 1;
            exr.ExceptionInformation[0] = reinterpret_cast<ULONG_PTR>(exception_text);
            FailFast(&exr);
        }
    }
    inline
    __analysis_noreturn
    void __cdecl AlwaysFatalConditionVa(_Printf_format_string_ const wchar_t* _text, _In_ va_list _argptr) NOEXCEPT
    {
        FatalConditionVa(true, _text, _argptr);
    }
    inline
    void __cdecl FatalCondition(bool _condition, _Printf_format_string_ const wchar_t* _text, ...) NOEXCEPT
    {
        if (_condition) {
            va_list argptr;
            va_start(argptr, _text);
            FatalConditionVa(_condition, _text, argptr);
            va_end(argptr);
        }
    }
    inline
    __analysis_noreturn
    void __cdecl AlwaysFatalCondition(_Printf_format_string_ const wchar_t* _text, ...) NOEXCEPT
    {
        va_list argptr;
        va_start(argptr, _text);
        FatalConditionVa(true, _text, argptr);
        va_end(argptr);
    }
    ///
    /// If enabled Runtime Type Information, will detect which exception based off the actual type
    /// - this is the /GR flag to the compiler
    /// - in the sources makefile: USE_RTTI = 1
    ///
    inline
    __analysis_noreturn
    void FatalCondition(const Exception& _exception) NOEXCEPT
    {
        FatalCondition(
            true,
            L"Exception : %s at %s [%u / 0x%x - %s]",
            _exception.what_w(),
            _exception.where_w(),
            _exception.why(),
            _exception.why(),
            _exception.translation_w());
    }

#ifdef _CPPRTTI
    inline
    __analysis_noreturn
    void FatalCondition(const std::exception& _exception) NOEXCEPT
    {
        const Exception* ntEx = dynamic_cast<const Exception*>(&_exception);
        if (ntEx != nullptr) {
            FatalCondition(*ntEx);
        } else {
            FatalCondition(
                true,
                L"std::exception : %S",
                _exception.what());
        }
    }
#else
    inline
    __analysis_noreturn
    void FatalCondition(const std::exception& _exception) NOEXCEPT
    {
        FatalCondition(
            true,
            L"std::exception : %S",
            _exception.what());
    }
#endif

}  // namespace ntl

