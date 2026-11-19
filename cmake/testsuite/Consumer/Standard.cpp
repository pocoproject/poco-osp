//
// Standard.cpp
//
// Compiled with the usage requirements of an installed OSP library in a project that sets a standard below C++20.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


// MSVC sets __cplusplus only with /Zc:__cplusplus.
#if defined(_MSVC_LANG)
static_assert(_MSVC_LANG >= 202002L, "the OSP library does not require C++20");
#else
static_assert(__cplusplus >= 202002L, "the OSP library does not require C++20");
#endif
