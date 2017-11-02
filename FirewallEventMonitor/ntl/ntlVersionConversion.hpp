// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

//
// using a macro to express no-throw behavior until noexcept is available in a released version of Visual C++
//
#if (_MSC_VER > 1800)
#define NOEXCEPT noexcept
#else
#define NOEXCEPT throw()
#endif
