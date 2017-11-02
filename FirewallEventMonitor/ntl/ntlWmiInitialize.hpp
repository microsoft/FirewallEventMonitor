// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

///
/// Compilation units must link against the following library when including Wmi* headers:
/// - wbemuuid.lib
///

/// load all Wmi headers in to Initialize
#include "ntlWmiService.hpp"
#include "ntlWmiClassObject.hpp"
#include "ntlWmiInstance.hpp"
#include "ntlWmiEnumerate.hpp"
#include "ntlWmiException.hpp"
#include "ntlWmiMakeVariant.hpp"
