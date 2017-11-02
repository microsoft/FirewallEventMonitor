// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

// cpp headers
#include <vector>
#include <string>

// os headers
#include <Windows.h>
#include <Objbase.h>

// ntl headers
#include "ntlComInitialize.hpp"
#include "ntlWmiInstance.hpp"


namespace ntl {

    //////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// WmiMakeVariant() functions are specializations designed to help
    ///   callers who want a way to construct a VARIANT (ComVariant) that is 
    ///   safe for passing into WMI - since WMI has limitations on what VARIANT
    ///   types it accepts.
    ///
    /// Note: all explicit specializations marked inline to avoid ODR violations
    ///
    //////////////////////////////////////////////////////////////////////////////////////////

    inline ntl::ComVariant WmiMakeVariant(bool t)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_BOOL>(t);
    }

    inline ntl::ComVariant WmiMakeVariant(char _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_UI1>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(unsigned char _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_UI1>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(short _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_I2>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(unsigned short _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_I2>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(long _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_I4>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(unsigned long _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_I4>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(int _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_I4>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(unsigned int _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_I4>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(float _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_R4>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(double _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_R8>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(SYSTEMTIME _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_DATE>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(BSTR _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_BSTR>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(LPCWSTR _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_BSTR>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(std::vector<std::wstring>& _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_BSTR | VT_ARRAY>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(std::vector<unsigned long>& _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_UI4 | VT_ARRAY>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(std::vector<unsigned short>& _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_UI2 | VT_ARRAY>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(std::vector<unsigned char>& _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign<VT_UI1 | VT_ARRAY>(_vtProp);
    }

    inline ntl::ComVariant WmiMakeVariant(WmiInstance& _vtProp)
    {
        ntl::ComVariant local_variant;
        return local_variant.assign(_vtProp.get_instance());
    }

    inline ntl::ComVariant WmiMakeVariant(std::vector<WmiInstance>& _vtProp)
    {
        ntl::ComVariant local_variant;
        std::vector<ntl::ComPtr<IWbemClassObject>> local_prop;
        for (const auto& prop : _vtProp) {
            local_prop.push_back(prop.get_instance());
        }
        return local_variant.assign(local_prop);
    }
}
