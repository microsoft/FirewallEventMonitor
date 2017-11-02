// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <wchar.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>
// os headers
#include <windows.h>
#include <Wbemidl.h>
// local headers
#include "ntlComInitialize.hpp"
#include "ntlWmiService.hpp"
#include "ntlWmiClassObject.hpp"
#include "ntlWmiException.hpp"
#include "ntlVersionConversion.hpp"


namespace ntl {

    class WmiInstance
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// Constructors:
        /// - requires a IWbemServices object already connected to WMI
        ///   
        /// - one c'tor creates an empty instance (if set later)
        /// - one c'tor takes the WMI class name to instantiate a new instance
        /// - one c'tor takes an existing IWbemClassObject instance
        ///
        /// Default d'tor, copy c'tor, and copy assignment operator
        ///
        ////////////////////////////////////////////////////////////////////////////////
        explicit WmiInstance(const WmiService& _wbemServices) : 
            wbemServices(_wbemServices),
            instanceObject()
        {
        }
        WmiInstance(const WmiService& _wbemServices, _In_ LPCWSTR _className) : 
            wbemServices(_wbemServices),
            instanceObject()
        {
            create_instance(_className);
        }
        WmiInstance(const WmiService& _wbemServices, const ntl::ComPtr<IWbemClassObject>& _instance) NOEXCEPT : 
            wbemServices(_wbemServices),
            instanceObject(_instance)
        {
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// comparison operators
        ///
        ////////////////////////////////////////////////////////////////////////////////
        bool operator ==(const WmiInstance& _obj) const NOEXCEPT
        {
            return ((this->wbemServices == _obj.wbemServices) &&
                    (this->instanceObject == _obj.instanceObject));
        }
        bool operator !=(const WmiInstance& _obj) const NOEXCEPT
        {
            return !(*this == _obj);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// Accessor to retrieve the encapsulated IWbemClassObject
        ///
        /// - returns the IWbemClassObject holding the instantiated class instance
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ntl::ComPtr<IWbemClassObject> get_instance() const NOEXCEPT
        {
            return this->instanceObject;
        }

        ntl::ComBstr path() const
        {
            ntl::ComVariant object_path_var;
            ntl::ComBstr object_path_bstr;

            this->get(L"__RELPATH", &object_path_var);
            if (!object_path_var.is_empty() && !object_path_var.is_null()) {
                object_path_var.retrieve(&object_path_bstr);
            }

            return object_path_bstr;
        }

        WmiService get_service() const NOEXCEPT
        {
            return this->wbemServices;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Retrieves the class name this WmiInstance is representing if any
        ///
        ////////////////////////////////////////////////////////////////////////////////
        ntl::ComBstr get_class_name() const
        {
            ntl::ComVariant class_var;
            ntl::ComBstr class_name;

            this->get(L"__CLASS", &class_var);
            if (!class_var.is_empty() && !class_var.is_null()) {
                class_var.retrieve(&class_name);
            }

            return class_name;
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Returns a class object for the class represented by this instance
        ///
        ////////////////////////////////////////////////////////////////////////////////
        WmiClassObject get_class_object() const
        {
            return WmiClassObject(this->wbemServices, this->instanceObject);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Writes the instantiated object to the WMI repository
        ///
        /// - supported _wbemFlags:
        ///   WBEM_FLAG_CREATE_OR_UPDATE
        ///   WBEM_FLAG_UPDATE_ONLY
        ///   WBEM_FLAG_CREATE_ONLY
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void write_instance(_In_opt_ const ntl::ComPtr<IWbemContext>& _context, const LONG _wbemFlags = WBEM_FLAG_CREATE_OR_UPDATE)
        {
            ntl::ComPtr<IWbemCallResult> result;
            // forced to const-cast to make this const-correct as COM does not have
            //   an expression for saying a method call is const
            HRESULT hr = this->wbemServices->PutInstance(
                this->instanceObject.get(),
                _wbemFlags | WBEM_FLAG_RETURN_IMMEDIATELY,
                const_cast<IWbemContext*>(_context.get()),
                result.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemServices::PutInstance", L"WmiInstance::write_instance", false);
            }

            // wait for the call to complete
            HRESULT status;
            hr = result->GetCallStatus(WBEM_INFINITE, &status);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemCallResult::GetCallStatus", L"WmiInstance::write_instance", false);
            }
            if (FAILED(status)) {
                throw ntl::WmiException(status, this->instanceObject.get(), L"IWbemServices::PutInstance", L"WmiInstance::write_instance", false);
            }
        }
        void write_instance(const LONG _wbemFlags = WBEM_FLAG_CREATE_OR_UPDATE)
        {
            ntl::ComPtr<IWbemContext> null_context;
            write_instance(null_context, _wbemFlags);
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Deletes the WMI object matching the instantiated IWbemClassObject
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void delete_instance()
        {
            ntl::ComBstr bstrObjectPath(this->path());
            //
            // delete the instance based off the __REPATH property
            //
            ntl::ComPtr<IWbemCallResult> result;
            HRESULT hr = this->wbemServices->DeleteInstance(
                bstrObjectPath.get(),
                WBEM_FLAG_RETURN_IMMEDIATELY,
                nullptr,
                result.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemServices::DeleteInstance", L"WmiInstance::delete_instance", false);
            }

            // wait for the call to complete
            HRESULT status;
            hr = result->GetCallStatus(WBEM_INFINITE, &status);
            if (FAILED(hr)) {
                // semi-sync call aborted - can't get status of the call
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemCallResult::GetCallStatus", L"WmiInstance::delete_instance", false);
            }
            if (FAILED(status)) {
                throw ntl::WmiException(status, this->instanceObject.get(), L"IWbemServices::DeleteInstance", L"WmiInstance::delete_instance", false);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Executes an instance method from the WMI object matching the 
        ///   instantiated IWbemClassObject
        ///
        /// - Invokes method with zero arguments
        ///
        /// - Returns a WmiInstace containing the [out] parameters
        ///   The property "ReturnValue" contains the return value of the method
        ///
        /// execute_method is *not* a const method as this class cannot guarantee
        ///    any method executed won't have other modifying side-effects
        ///
        ////////////////////////////////////////////////////////////////////////////////
        WmiInstance execute_method(_In_ LPCWSTR _method)
        {
            return this->execute_method_private(_method, nullptr);
        }
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Executes an instance method from the WMI object matching the 
        ///   instantiated IWbemClassObject
        ///
        /// - Invokes method with one argument
        ///
        /// - Returns a WmiInstace containing the [out] parameters
        ///   The property "ReturnValue" contains the return value of the method
        ///
        /// execute_method is *not* a const method as this class cannot guarantee
        ///    any method executed won't have other modifying side-effects
        ///
        ////////////////////////////////////////////////////////////////////////////////
        template <typename Arg1>
        WmiInstance execute_method(_In_ LPCWSTR _method, Arg1 _arg1)
        {
            //
            // establish the class object for the [in] params to the method
            //
            ntl::ComPtr<IWbemClassObject> inParamsDefinition;
            HRESULT hr = this->instanceObject->GetMethod(
                _method, 0, inParamsDefinition.get_addr_of(), nullptr);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::GetMethod", L"WmiInstance::exec_method", false);
            }
            //
            // spawn an instance to store the params
            //
            ntl::ComPtr<IWbemClassObject> inParamsInstance;
            hr = inParamsDefinition->SpawnInstance(0, inParamsInstance.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, L"IWbemClassObject::SpawnInstance", L"WmiInstance::exec_method", false);
            }
            //
            // Instantiate a class object to iterate through each property
            //
            WmiClassObject property_object(this->wbemServices, inParamsDefinition);
            auto property_iter = property_object.property_begin();
            //
            // write each property
            //
            WmiInstance property_instance(this->wbemServices, inParamsInstance);
            property_instance.set(property_iter->get(), _arg1);
            //
            // execute the method with the properties set
            //
            return this->execute_method_private(_method, inParamsInstance.get());
        }
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Executes an instance method from the WMI object matching the 
        ///   instantiated IWbemClassObject
        ///
        /// - Invokes method with two arguments
        ///
        /// - Returns a WmiInstace containing the [out] parameters
        ///   The property "ReturnValue" contains the return value of the method
        ///
        /// execute_method is *not* a const method as this class cannot guarantee
        ///    any method executed won't have other modifying side-effects
        ///
        ////////////////////////////////////////////////////////////////////////////////
        template <typename Arg1, typename Arg2>
        WmiInstance execute_method(_In_ LPCWSTR _method, Arg1 _arg1, Arg2 _arg2)
        {
            //
            // establish the class object for the [in] params to the method
            //
            ntl::ComPtr<IWbemClassObject> inParamsDefinition;
            HRESULT hr = this->instanceObject->GetMethod(
                _method, 0, inParamsDefinition.get_addr_of(), nullptr);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::GetMethod", L"WmiInstance::exec_method", false);
            }
            //
            // spawn an instance to store the params
            //
            ntl::ComPtr<IWbemClassObject> inParamsInstance;
            hr = inParamsDefinition->SpawnInstance(0, inParamsInstance.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, L"IWbemClassObject::SpawnInstance", L"WmiInstance::exec_method", false);
            }
            //
            // Instantiate a class object to iterate through each property
            //
            WmiClassObject property_object(this->wbemServices, inParamsDefinition);
            auto property_iter = property_object.property_begin();
            //
            // write each property
            //
            WmiInstance property_instance(this->wbemServices, inParamsInstance);
            property_instance.set(property_iter->get(), _arg1);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg2);
            //
            // execute the method with the properties set
            //
            return this->execute_method_private(_method, inParamsInstance.get());
        }
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Executes an instance method from the WMI object matching the 
        ///   instantiated IWbemClassObject
        ///
        /// - Invokes method with three arguments
        ///
        /// - Returns a WmiInstace containing the [out] parameters
        ///   The property "ReturnValue" contains the return value of the method
        ///
        /// execute_method is *not* a const method as this class cannot guarantee
        ///    any method executed won't have other modifying side-effects
        ///
        ////////////////////////////////////////////////////////////////////////////////
        template <typename Arg1, typename Arg2, typename Arg3>
        WmiInstance execute_method(_In_ LPCWSTR _method, Arg1 _arg1, Arg2 _arg2, Arg3 _arg3)
        {
            //
            // establish the class object for the [in] params to the method
            //
            ntl::ComPtr<IWbemClassObject> inParamsDefinition;
            HRESULT hr = this->instanceObject->GetMethod(
                _method, 0, inParamsDefinition.get_addr_of(), nullptr);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::GetMethod", L"WmiInstance::exec_method", false);
            }
            //
            // spawn an instance to store the params
            //
            ntl::ComPtr<IWbemClassObject> inParamsInstance;
            hr = inParamsDefinition->SpawnInstance(0, inParamsInstance.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, L"IWbemClassObject::SpawnInstance", L"WmiInstance::exec_method", false);
            }
            //
            // Instantiate a class object to iterate through each property
            //
            WmiClassObject property_object(this->wbemServices, inParamsDefinition);
            auto property_iter = property_object.property_begin();
            //
            // write each property
            //
            WmiInstance property_instance(this->wbemServices, inParamsInstance);
            property_instance.set(property_iter->get(), _arg1);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg2);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg3);
            //
            // execute the method with the properties set
            //
            return this->execute_method_private(_method, inParamsInstance.get());
        }
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Executes an instance method from the WMI object matching the 
        ///   instantiated IWbemClassObject
        ///
        /// - Invokes method with four arguments
        ///
        /// - Returns a WmiInstace containing the [out] parameters
        ///   The property "ReturnValue" contains the return value of the method
        ///
        /// execute_method is *not* a const method as this class cannot guarantee
        ///    any method executed won't have other modifying side-effects
        ///
        ////////////////////////////////////////////////////////////////////////////////
        template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
        WmiInstance execute_method(_In_ LPCWSTR _method, Arg1 _arg1, Arg2 _arg2, Arg3 _arg3, Arg4 _arg4)
        {
            //
            // establish the class object for the [in] params to the method
            //
            ntl::ComPtr<IWbemClassObject> inParamsDefinition;
            HRESULT hr = this->instanceObject->GetMethod(
                _method, 0, inParamsDefinition.get_addr_of(), nullptr);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::GetMethod", L"WmiInstance::exec_method", false);
            }
            //
            // spawn an instance to store the params
            //
            ntl::ComPtr<IWbemClassObject> inParamsInstance;
            hr = inParamsDefinition->SpawnInstance(0, inParamsInstance.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, L"IWbemClassObject::SpawnInstance", L"WmiInstance::exec_method", false);
            }
            //
            // Instantiate a class object to iterate through each property
            //
            WmiClassObject property_object(this->wbemServices, inParamsDefinition);
            auto property_iter = property_object.property_begin();
            //
            // write each property
            //
            WmiInstance property_instance(this->wbemServices, inParamsInstance);
            property_instance.set(property_iter->get(), _arg1);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg2);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg3);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg4);
            //
            // execute the method with the properties set
            //
            return this->execute_method_private(_method, inParamsInstance.get());
        }
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// - Executes an instance method from the WMI object matching the 
        ///   instantiated IWbemClassObject
        ///
        /// - Invokes method with five arguments
        ///
        /// - Returns a WmiInstace containing the [out] parameters
        ///   The property "ReturnValue" contains the return value of the method
        ///
        /// execute_method is *not* a const method as this class cannot guarantee
        ///    any method executed won't have other modifying side-effects
        ///
        ////////////////////////////////////////////////////////////////////////////////
        template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
        WmiInstance execute_method(_In_ LPCWSTR _method, Arg1 _arg1, Arg2 _arg2, Arg3 _arg3, Arg4 _arg4, Arg5 _arg5)
        {
            //
            // establish the class object for the [in] params to the method
            //
            ntl::ComPtr<IWbemClassObject> inParamsDefinition;
            HRESULT hr = this->instanceObject->GetMethod(
                _method, 0, inParamsDefinition.get_addr_of(), nullptr);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::GetMethod", L"WmiInstance::exec_method", false);
            }
            //
            // spawn an instance to store the params
            //
            ntl::ComPtr<IWbemClassObject> inParamsInstance;
            hr = inParamsDefinition->SpawnInstance(0, inParamsInstance.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, L"IWbemClassObject::SpawnInstance", L"WmiInstance::exec_method", false);
            }
            //
            // Instantiate a class object to iterate through each property
            //
            WmiClassObject property_object(this->wbemServices, inParamsDefinition);
            auto property_iter = property_object.property_begin();
            //
            // write each property
            //
            WmiInstance property_instance(this->wbemServices, inParamsInstance);
            property_instance.set(property_iter->get(), _arg1);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg2);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg3);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg4);
            ++property_iter;
            property_instance.set(property_iter->get(), _arg5);
            //
            // execute the method with the properties set
            //
            return this->execute_method_private(_method, inParamsInstance.get());
        }

        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// get() - exposes the properties of the WMI object instantiated
        ///       - accepts types that can be passed to ntl::ComVariantl::retrieve
        ///       - WMI instances don't use all VARIANT types - some special get specializations
        ///         exist because, for example, 64-bit integers actually get passed through
        ///         WMI as BSTRs (even though variants support 64-bit integers directly).
        ///         See the MSDN documentation for WMI MOF Data Types (Numbers):
        ///         http://msdn.microsoft.com/en-us/library/aa392716(v=VS.85).aspx
        ///       - can throw under low resources of the type copied into needs allocation
        ///       - can throw if the VARIANT conversion fails after the WMI call succeeds
        ///       - a const method - as the only side-effect known is incrementing 
        ///         a refcount of an embedded COM object in the VARIANT
        ///
        ////////////////////////////////////////////////////////////////////////////////

        bool is_null(_In_ LPCWSTR _propname) const
        {
            ntl::ComVariant vtProp;
            this->get_impl(_propname, vtProp.get());
            return vtProp.is_null();
        }

        /// this has a complex annotation as T* can be a POD (like an int*) which must be _Out_
        /// or T* could be a non-POD (like a std::wstring*) which must be _Inout_
        template <typename T>
        void get(_In_ LPCWSTR _propname, _Out_ T* _t) const
        {
            ntl::ComVariant vtProp;
            this->get_impl(_propname, vtProp.get());
            ///
            /// if Get succeeds but the resulting VARIANT is NULL or Empty,
            /// - will throw with e.why() == S_FALSE
            ///
            if (vtProp.is_empty() || vtProp.is_null()) {
                wchar_t propName[128];
                ::_snwprintf_s(propName, 128, _TRUNCATE, L"Requested property %s is empty or null", _propname);
                throw ntl::WmiException(S_FALSE, this->instanceObject.get(), propName, L"WmiInstance::get", true);
            }
            // suppressing the 'Using uninitialized memory' warning since T* must be an _Out_ 
            // but retrieve can be _Inout_. This is an artifact in how retrieve works with 
            // both POD (_Out_) and non-POD (_Inout_) types
#pragma warning( suppress : 6001)
            vtProp.retrieve(_t);
        }
        ///
        /// Overriding the get() function template for retrieving VARIANT values directly
        /// - requires a properly initialized VARIANT
        ///
        void get(_In_ LPCWSTR _propname, _Inout_ VARIANT* _variant) const
        {
            ::VariantClear(_variant);
            this->get_impl(_propname, _variant);
        }
        void get(_In_ LPCWSTR _propname, _Inout_ ntl::ComVariant* _variant) const
        {
            ntl::ComVariant vtProp;
            this->get_impl(_propname, vtProp.get());
            _variant->swap(vtProp);
        }

        ///
        /// Overriding the get() function template to marshal a WMI instance from the underlying variant
        ///
        void get(_In_ LPCWSTR _propname, _Inout_ ntl::ComPtr<IWbemClassObject>* _instance) const
        {
            ntl::ComVariant vtProp;
            this->get_impl(_propname, vtProp.get());
            ///
            /// if Get succeeds but the resulting VARIANT is NULL or Empty,
            /// - will throw with e.why() == S_FALSE
            ///
            if (vtProp.is_empty() || vtProp.is_null()) {
                wchar_t propName[128];
                ::_snwprintf_s(propName, 128, _TRUNCATE, L"Requested property %s is empty or null", _propname);
                throw ntl::WmiException(S_FALSE, this->instanceObject.get(), propName, L"WmiInstance::get", true);
            }

            vtProp.retrieve<IWbemClassObject>(_instance);
        }
        void get(_In_ LPCWSTR _propname, _Inout_ WmiInstance* _instance) const
        {
            ntl::ComPtr<IWbemClassObject> instance_ptr(_instance->get_instance());
            this->get(_propname, &instance_ptr);

            *_instance = WmiInstance(_instance->get_service(), instance_ptr);
        }
        void get(_In_ LPCWSTR _propname, _Inout_ std::vector<ntl::ComPtr<IWbemClassObject>>* _instance) const
        {
            ntl::ComVariant vtProp;
            this->get_impl(_propname, vtProp.get());
            ///
            /// if Get succeeds but the resulting VARIANT is NULL or Empty,
            /// - will throw with e.why() == S_FALSE
            ///
            if (vtProp.is_empty() || vtProp.is_null()) {
                wchar_t propName[128];
                ::_snwprintf_s(propName, 128, _TRUNCATE, L"Requested property %s is empty or null", _propname);
                throw ntl::WmiException(S_FALSE, this->instanceObject.get(), propName, L"WmiInstance::get", true);
            }

            vtProp.retrieve<IWbemClassObject>(_instance);
        }

        ///
        /// Even though VARIANTs support 64-bit integers, WMI passes them around as BSTRs
        ///
        /// This does NOT do any checks to see whether the underlying BSTR is a valid number -
        /// if it's a BSTR but it didn't come from the relevant type of 64-bit integer, these
        /// getters will appear to succeed (the output value will be whatever _wcstoi64/_wcstoui64
        /// on the BSTR returns, normally the appropriate MAX or MIN value on overflow and 0 on other
        /// errors).
        ///
        void get(_In_ LPCWSTR _propname, _Out_ UINT64* _t) const
        {
            ntl::ComBstr intermediaryStr;
            this->get(_propname, &intermediaryStr);
            *_t = _wcstoui64(intermediaryStr.get(), nullptr, 10);
        }

        void get(_In_ LPCWSTR _propname, _Out_ INT64* _t) const
        {
            ntl::ComBstr intermediaryStr;
            this->get(_propname, &intermediaryStr);
            *_t = _wcstoi64(intermediaryStr.get(), nullptr, 10);
        }

        ///
        /// Even though VARIANTs support 16- and 32-bit unsigned integers, WMI passes them both 
        /// around as 32-bit signed integers. Yes, that means you can't pass very large UINT32 values
        /// correctly through WMI directly.
        ///
        /// The ntl::ComVariantl::retrieve method will correctly convert the 32-bit case, but not the
        /// 16-bit case (since it would normally risk losing information).
        ///
        /// This method does NOT do any overflow checking whatsoever. Be sure not to use this
        /// specialization on anything that actually is an INT32.
        ///
        void get(_In_ LPCWSTR _propname, _Out_ UINT16* _t) const
        {
            INT32 intermediaryInt32;
            this->get(_propname, &intermediaryInt32);
            *_t = static_cast<UINT16>(intermediaryInt32);
        }

        ////////////////////////////////////////////////////////////////////////////////////
        ///
        /// set() - allows writing to the WMI object instantiated
        ///       - accepts types that are convertable to a ntl::ComVariant
        ///       - only specific VARIANT Types are supported through WMI
        ///         this template provides that filter
        ///       - *not* a const method
        ///       - WMI also supports VT_NULL, VT_DISPATCH and VT_UNKNOWN
        ///         - these are not yet implemented.
        ///
        ////////////////////////////////////////////////////////////////////////////////////
        void set(_In_ LPCWSTR _propname, const VARIANT* _vtProp)
        {
            // IWbemClassObject::Put should have declared the VARIANT member const
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, const_cast<VARIANT*>(_vtProp), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const bool _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_BOOL>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const char _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_UI1>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const unsigned char _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_UI1>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const short _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_I2>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const unsigned short _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_I2>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const long _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_I4>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const unsigned long _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_I4>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const int _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_I4>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const unsigned int _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_I4>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const float _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_R4>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const double _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_R8>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const SYSTEMTIME _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_DATE>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const BSTR _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_BSTR>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, _In_ LPCWSTR _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_BSTR>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }
        void set(_In_ LPCWSTR _propname, const std::vector<std::wstring>& _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_BSTR | VT_ARRAY>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"WmiInstance::set", false);
            }
        }

        void set(_In_ LPCWSTR _propname, const std::vector<unsigned long>& _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_UI4 | VT_ARRAY>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"ntl::WmiInstance::set", false);
            }
        }

        void set(_In_ LPCWSTR _propname, const std::vector<unsigned char>& _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_UI1 | VT_ARRAY>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"ntl::WmiInstance::set", false);
            }
        }

        void set(_In_ LPCWSTR _propname, const std::vector<unsigned short>& _vtProp)
        {
            ntl::ComVariant local_variant;
            local_variant.assign<VT_UI2 | VT_ARRAY>(_vtProp);
            HRESULT hr = this->instanceObject->Put(
                _propname, 0, local_variant.get(), 0);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Put", L"ntl::WmiInstance::set", false);
            }
        }
        ////////////////////////////////////////////////////////////////////////////////
        ///
        /// void set_default(LPCWSTR)
        ///
        /// - Calling IWbemClassObject::Delete on a property of an instance resets
        ///   to the default value.
        ///
        ////////////////////////////////////////////////////////////////////////////////
        void set_default(_In_ LPCWSTR _propname)
        {
            HRESULT hr = this->instanceObject->Delete(_propname);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Delete", L"WmiInstance::set_default", false);
            }

        }

    private:
        WmiService wbemServices;
        ntl::ComPtr<IWbemClassObject> instanceObject;

        void get_impl(_In_ LPCWSTR _propname, _Inout_ VARIANT* _variant) const
        {
            // since COM doesn't support marking methods const, calls to Get() are const_cast out of necessity
            HRESULT hr = const_cast<IWbemClassObject*>(this->instanceObject.get())->Get(
                _propname, 0, _variant, nullptr, nullptr);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::Get", L"WmiInstance::get", false);
            }
        }
        void create_instance(_In_ LPCWSTR _className)
        {
            ntl::ComBstr classBstr(_className);
            ntl::ComPtr<IWbemClassObject> classObject;
            // get the object from the WMI service
            HRESULT hr = this->wbemServices->GetObject(
                classBstr.get(), 0, nullptr, classObject.get_addr_of(), nullptr);
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemServices::GetObject", L"WmiInstance::WmiInstance", false);
            }

            // spawn an instance of this object
            hr = classObject->SpawnInstance(
                0, this->instanceObject.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, this->instanceObject.get(), L"IWbemClassObject::SpawnInstance", L"WmiInstance::WmiInstance", false);
            }
        }

        WmiInstance execute_method_private(_In_ LPCWSTR _method, _In_opt_ IWbemClassObject* _inParams)
        {
            ntl::ComBstr bstrObjectPath(this->path());
            //
            // exec the method semi-synchronously from this instance based off the __REPATH property
            //
            ntl::ComPtr<IWbemCallResult> result;
            HRESULT hr = this->wbemServices->ExecMethod(
                bstrObjectPath.get(),
                ntl::ComBstr(_method).get(),
                WBEM_FLAG_RETURN_IMMEDIATELY,
                nullptr,
                _inParams,
                nullptr,
                result.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, L"IWbemServices::ExecMethod", L"WmiInstance::execute_method", false);
            }
            //
            // wait for the call to complete - and get the [out] param object
            //
            ntl::ComPtr<IWbemClassObject> outParamsInstance;
            hr = result->GetResultObject(WBEM_INFINITE, outParamsInstance.get_addr_of());
            if (FAILED(hr)) {
                throw ntl::WmiException(hr, L"IWbemCallResult::GetResultObject", L"WmiInstance::execute_method", false);
            }
            //
            // the call went through - return the [out] params
            //
            return WmiInstance(this->wbemServices, outParamsInstance);
        }
    };

} // namespace ntl
